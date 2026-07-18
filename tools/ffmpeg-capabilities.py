#!/usr/bin/env python3
"""Generate and audit the project's manifest-controlled FFmpeg feature set."""

from __future__ import annotations

import argparse
import fnmatch
import json
import pathlib
import re
import shutil
import subprocess
import sys
from collections.abc import Iterable

ROOT = pathlib.Path(__file__).resolve().parents[1]
DEFAULT_MANIFEST = ROOT / "tools" / "manifests" / "ffmpeg-capabilities.json"
CATEGORIES = {
    "protocols": "protocol",
    "demuxers": "demuxer",
    "parsers": "parser",
    "decoders": "decoder",
    "encoders": "encoder",
    "filters": "filter",
    "muxers": "muxer",
    "bitstreamFilters": "bsf",
}
MESON_FILTER_NAMES = {
    "abuffer": "asrc_abuffer",
    "abuffersink": "asink_abuffer",
    "buffer": "vsrc_buffer",
    "buffersink": "vsink_buffer",
}
RUNTIME_DEMUXER_NAMES = {
    "mpegps": "mpeg",
    "pcm_s16le": "s16le",
    "pcm_s24le": "s24le",
    "pcm_s32le": "s32le",
}
RUNTIME_DECODER_NAMES = {
    "movtext": "mov_text",
}


def load_manifest(path: pathlib.Path) -> dict:
    with path.open(encoding="utf-8") as handle:
        data = json.load(handle)
    if data.get("schemaVersion") != 1:
        raise ValueError("unsupported FFmpeg capability manifest schema")
    required = {
        "--disable-everything",
        "--disable-gpl",
        "--disable-version3",
        "--disable-nonfree",
        "--disable-autodetect",
    }
    if set(data.get("requiredDisableFlags", [])) != required:
        raise ValueError("requiredDisableFlags must contain the complete license/autodetect baseline")
    for key in ("libraries", *CATEGORIES):
        values = data.get(key)
        if not isinstance(values, list) or any(not isinstance(value, str) or not value for value in values):
            raise ValueError(f"{key} must be a list of non-empty strings")
        if values != sorted(set(values)):
            raise ValueError(f"{key} must be sorted and contain no duplicates")
    image_decoders = data.get("forbiddenImageDecoders")
    if not isinstance(image_decoders, list) or image_decoders != sorted(set(image_decoders)):
        raise ValueError("forbiddenImageDecoders must be sorted and contain no duplicates")
    enabled_images = set(image_decoders) & set(data["decoders"])
    if enabled_images:
        raise ValueError(f"FFmpeg image decoders must remain disabled: {sorted(enabled_images)}")
    for platform in ("linux", "macos", "webos", "windows"):
        if platform not in data.get("platforms", {}):
            raise ValueError(f"missing platform capability set: {platform}")
        hardware_accelerators = data["platforms"][platform].get("hardwareAccelerators")
        if (
            not isinstance(hardware_accelerators, list)
            or hardware_accelerators != sorted(set(hardware_accelerators))
        ):
            raise ValueError(f"{platform} hardwareAccelerators must be sorted and contain no duplicates")
    forbidden = set(data.get("forbiddenConfigureFlags", []))
    emitted = set(configure_flags(data, "webos"))
    overlap = forbidden & emitted
    if overlap:
        raise ValueError(f"forbidden configure flags are enabled: {sorted(overlap)}")
    return data


def platform_protocols(data: dict, platform: str) -> list[str]:
    return sorted(set(data["protocols"]) | set(data["platforms"][platform].get("protocols", [])))


def configure_flags(data: dict, platform: str) -> list[str]:
    if platform not in data["platforms"]:
        raise ValueError(f"unknown FFmpeg platform: {platform}")
    flags = list(data["requiredDisableFlags"])
    flags.extend(data["commonConfigureFlags"])
    flags.extend(data["platforms"][platform].get("configureFlags", []))
    flags.extend(f"--enable-{library}" for library in data["libraries"])
    for key, configure_name in CATEGORIES.items():
        values = platform_protocols(data, platform) if key == "protocols" else data[key]
        flags.extend(f"--enable-{configure_name}={value}" for value in values)
    flags.extend(
        f"--enable-hwaccel={value}" for value in data["platforms"][platform]["hardwareAccelerators"]
    )
    return flags


def meson_flags(data: dict, platform: str) -> list[str]:
    if platform != "windows":
        raise ValueError("the Meson FFmpeg fallback is currently Windows-only")
    flags = [
        "ffmpeg:default_library=static",
        "ffmpeg:auto_features=disabled",
        "ffmpeg:gpl=disabled",
        "ffmpeg:version3=disabled",
        "ffmpeg:nonfree=disabled",
        "ffmpeg:programs=disabled",
        "ffmpeg:tests=disabled",
        "ffmpeg:avdevice=disabled",
        "ffmpeg:postproc=disabled",
        "ffmpeg:w32threads=enabled",
        "ffmpeg:x86asm=enabled",
        "ffmpeg:network=disabled",
        "ffmpeg:d3d11va=enabled",
        "ffmpeg:dxva2=enabled",
    ]
    flags.extend(f"ffmpeg:{library}=enabled" for library in data["libraries"])
    for key, suffix in CATEGORIES.items():
        values = platform_protocols(data, platform) if key == "protocols" else data[key]
        for value in values:
            option = MESON_FILTER_NAMES.get(value, value) if key == "filters" else value
            flags.append(f"ffmpeg:{option}_{suffix}=enabled")
    flags.extend(
        f"ffmpeg:{value}_hwaccel=enabled" for value in data["platforms"][platform]["hardwareAccelerators"]
    )
    flags.extend(("ffmpeg:sdl2=disabled", "ffmpeg:bzlib=disabled", "ffmpeg:iconv=disabled", "ffmpeg:lzma=disabled"))
    return flags
def split_enabled_values(configuration: str) -> Iterable[tuple[str, str]]:
    pattern = re.compile(
        r"--enable-(protocol|demuxer|parser|decoder|encoder|filter|muxer|bsf|hwaccel)=([^\s'\"]+)"
    )
    for match in pattern.finditer(configuration):
        for value in match.group(2).split(","):
            yield match.group(1), value

def split_enabled_components(configuration: str) -> Iterable[tuple[str, str]]:
    suffixes = {
        "PROTOCOL": "protocol",
        "DEMUXER": "demuxer",
        "PARSER": "parser",
        "DECODER": "decoder",
        "ENCODER": "encoder",
        "FILTER": "filter",
        "MUXER": "muxer",
        "BSF": "bsf",
        "HWACCEL": "hwaccel",
    }
    pattern = re.compile(
        r"^#define CONFIG_(.+)_(PROTOCOL|DEMUXER|PARSER|DECODER|ENCODER|FILTER|MUXER|BSF|HWACCEL) 1$",
        re.MULTILINE,
    )
    for match in pattern.finditer(configuration):
        yield suffixes[match.group(2)], match.group(1).lower()


def cpp_header(data: dict, platform: str) -> str:
    arrays = {
        "Protocols": platform_protocols(data, platform),
        "Demuxers": [RUNTIME_DEMUXER_NAMES.get(value, value) for value in data["demuxers"]],
        "Decoders": [RUNTIME_DECODER_NAMES.get(value, value) for value in data["decoders"]],
        "Encoders": data["encoders"],
        "Filters": data["filters"],
        "Muxers": data["muxers"],
        "BitstreamFilters": data["bitstreamFilters"],
        "ForbiddenImageDecoders": data["forbiddenImageDecoders"],
    }
    lines = [
        "#pragma once",
        "",
        "#include <array>",
        "#include <string_view>",
        "",
        "namespace FfmpegCapabilities {",
        f'inline constexpr std::string_view kPlatform = "{platform}";',
    ]
    for name, values in arrays.items():
        lines.append(f"inline constexpr std::array<std::string_view, {len(values)}> k{name} {{")
        lines.extend(f'    "{value}",' for value in values)
        lines.append("};")
    lines.extend(("} // namespace FfmpegCapabilities", ""))
    return "\n".join(lines)


def audit_configuration(data: dict, platform: str, configuration: str) -> None:
    missing = [flag for flag in data["requiredDisableFlags"] if flag not in configuration]
    if missing:
        raise ValueError(f"effective FFmpeg configuration is missing: {', '.join(missing)}")
    present_forbidden = [flag for flag in data["forbiddenConfigureFlags"] if flag in configuration]
    if present_forbidden:
        raise ValueError(f"effective FFmpeg configuration enables forbidden licensing: {', '.join(present_forbidden)}")
    allowed: dict[str, set[str]] = {}
    for key, configure_name in CATEGORIES.items():
        values = platform_protocols(data, platform) if key == "protocols" else data[key]
        allowed[configure_name] = set(values)
    allowed["hwaccel"] = set(data["platforms"][platform]["hardwareAccelerators"])
    enabled = list(split_enabled_values(configuration))
    enabled.extend(split_enabled_components(configuration))
    unexpected = sorted(
        f"{category}={value}" for category, value in enabled if value not in allowed[category]
    )
    if unexpected:
        raise ValueError(f"effective FFmpeg configuration enables unlisted features: {', '.join(unexpected)}")


def candidate_files(paths: list[pathlib.Path]) -> Iterable[pathlib.Path]:
    for path in paths:
        if path.is_file():
            yield path
        elif path.is_dir():
            yield from (candidate for candidate in path.rglob("*") if candidate.is_file())


def audit_closure(data: dict, paths: list[pathlib.Path]) -> None:
    patterns = data["forbiddenLibraryPatterns"]
    readelf = shutil.which("readelf")
    violations: set[str] = set()
    for path in candidate_files(paths):
        if any(fnmatch.fnmatch(path.name, pattern) for pattern in patterns):
            violations.add(str(path))
        if not readelf:
            continue
        result = subprocess.run([readelf, "-d", path], text=True, capture_output=True, check=False)
        if result.returncode != 0:
            continue
        for dependency in re.findall(r"Shared library: \[([^]]+)]", result.stdout):
            if any(fnmatch.fnmatch(dependency, pattern) for pattern in patterns):
                violations.add(f"{path}: NEEDED {dependency}")
    if violations:
        raise ValueError("forbidden FFmpeg dependency closure entries:\n" + "\n".join(sorted(violations)))


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--manifest", type=pathlib.Path, default=DEFAULT_MANIFEST)
    subparsers = parser.add_subparsers(dest="command", required=True)

    for command in ("configure", "meson"):
        child = subparsers.add_parser(command)
        child.add_argument("--platform", required=True, choices=("linux", "macos", "webos", "windows"))

    audit = subparsers.add_parser("audit-config")
    audit.add_argument("--platform", required=True, choices=("linux", "macos", "webos", "windows"))
    audit.add_argument("configuration", nargs="+", type=pathlib.Path)

    closure = subparsers.add_parser("audit-closure")
    closure.add_argument("paths", nargs="+", type=pathlib.Path)

    header = subparsers.add_parser("cpp-header")
    header.add_argument("--platform", required=True, choices=("linux", "macos", "webos", "windows"))
    header.add_argument("--output", required=True, type=pathlib.Path)

    subparsers.add_parser("validate")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    try:
        data = load_manifest(args.manifest)
        if args.command == "configure":
            print("\n".join(configure_flags(data, args.platform)))
        elif args.command == "meson":
            print("\n".join(meson_flags(data, args.platform)))
        elif args.command == "audit-config":
            configuration = "\n".join(
                path.read_text(encoding="utf-8", errors="replace") for path in args.configuration
            )
            audit_configuration(data, args.platform, configuration)
        elif args.command == "cpp-header":
            args.output.parent.mkdir(parents=True, exist_ok=True)
            args.output.write_text(cpp_header(data, args.platform), encoding="utf-8")
        elif args.command == "audit-closure":
            audit_closure(data, args.paths)
    except (OSError, ValueError, json.JSONDecodeError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
