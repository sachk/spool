#!/usr/bin/env python3
from __future__ import annotations

import json
import subprocess
import sys
import tempfile
from pathlib import Path


def run(script: Path, manifest: Path, *args: str, expected: int = 0) -> subprocess.CompletedProcess[str]:
    result = subprocess.run(
        [sys.executable, str(script), "--manifest", str(manifest), *args],
        text=True,
        capture_output=True,
        check=False,
    )
    if result.returncode != expected:
        raise AssertionError(
            f"command returned {result.returncode}, expected {expected}: {' '.join(args)}\n"
            f"stdout:\n{result.stdout}\nstderr:\n{result.stderr}"
        )
    return result


def main() -> int:
    if len(sys.argv) != 3:
        raise SystemExit("usage: FfmpegCapabilitiesTest.py <generator> <manifest>")
    script = Path(sys.argv[1])
    manifest = Path(sys.argv[2])

    run(script, manifest, "validate")
    required = {
        "--disable-everything",
        "--disable-gpl",
        "--disable-version3",
        "--disable-nonfree",
        "--disable-autodetect",
    }
    for platform in ("linux", "macos", "webos"):
        flags = set(run(script, manifest, "configure", "--platform", platform).stdout.splitlines())
        assert required <= flags, (platform, required - flags)
        assert "--enable-gpl" in flags, platform
        assert not ({"--enable-version3", "--enable-nonfree"} & flags), platform
        assert "--enable-decoder=mjpeg" not in flags, platform
        assert "--enable-decoder=png" not in flags, platform
        assert "--enable-decoder=webp" not in flags, platform

    meson = set(run(script, manifest, "meson", "--platform", "windows").stdout.splitlines())
    assert {"ffmpeg:gpl=enabled", "ffmpeg:version3=disabled", "ffmpeg:nonfree=disabled"} <= meson
    assert "ffmpeg:mjpeg_decoder=enabled" not in meson
    assert "ffmpeg:png_decoder=enabled" not in meson
    assert "ffmpeg:webp_decoder=enabled" not in meson

    with tempfile.TemporaryDirectory() as directory:
        config = Path(directory) / "config.log"
        components = Path(directory) / "config_components.h"
        config.write_text(" ".join(sorted(required | {"--enable-gpl"})) + "\n", encoding="utf-8")
        components.write_text(
            "#define CONFIG_H264_DECODER 1\n#define CONFIG_FILE_PROTOCOL 1\n",
            encoding="utf-8",
        )

        lgpl_manifest = Path(directory) / "ffmpeg-capabilities-lgpl.json"
        lgpl_data = json.loads(manifest.read_text(encoding="utf-8"))
        lgpl_data["platforms"]["macos"]["gpl"] = False
        lgpl_manifest.write_text(json.dumps(lgpl_data), encoding="utf-8")
        lgpl_flags = set(run(script, lgpl_manifest, "configure", "--platform", "macos").stdout.splitlines())
        assert "--disable-gpl" in lgpl_flags
        assert "--enable-gpl" not in lgpl_flags
        lgpl_header = Path(directory) / "FfmpegCapabilities.h"
        run(script, lgpl_manifest, "cpp-header", "--platform", "macos", "--output", str(lgpl_header))
        assert "inline constexpr bool kGplEnabled = false;" in lgpl_header.read_text(encoding="utf-8")
        run(script, manifest, "audit-config", "--platform", "webos", str(config), str(components))
        components.write_text("#define CONFIG_MJPEG_DECODER 1\n", encoding="utf-8")
        rejected = run(
            script,
            manifest,
            "audit-config",
            "--platform",
            "webos",
            str(config),
            str(components),
            expected=1,
        )
        assert "unlisted features" in rejected.stderr

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
