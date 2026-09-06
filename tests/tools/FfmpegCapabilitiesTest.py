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
    for platform in ("linux", "macos", "webos", "windows", "android"):
        flags = set(run(script, manifest, "configure", "--platform", platform).stdout.splitlines())
        assert required <= flags, (platform, required - flags)
        # The app is MPL-2.0 and nothing in the component set is GPL-only, so
        # FFmpeg builds as LGPL-2.1-or-later. That is also what lets it link
        # the OpenSSL the cross builds already carry: FFmpeg refuses OpenSSL
        # under --enable-gpl without --enable-version3.
        assert "--enable-gpl" not in flags, platform
        assert not ({"--enable-version3", "--enable-nonfree"} & flags), platform
        assert "--enable-decoder=mjpeg" not in flags, platform
        assert "--enable-decoder=png" not in flags, platform
        assert "--enable-decoder=webp" not in flags, platform
        # lavf's HLS demuxer resolves segment URLs by protocol name before mpv's
        # libcurl backend is consulted, so every platform has to register these.
        assert {"--enable-protocol=https", "--enable-protocol=tls"} <= flags, platform

    for platform in ("android", "webos", "linux", "macos", "windows"):
        flags = set(run(script, manifest, "configure", "--platform", platform).stdout.splitlines())
        assert ("--enable-bsf=dovi_split" in flags) == (platform in ("linux", "macos", "windows"))
    windows = set(run(script, manifest, "configure", "--platform", "windows").stdout.splitlines())
    assert {"--enable-nvdec", "--enable-hwaccel=av1_nvdec", "--enable-hwaccel=hevc_d3d12va"} <= windows

    with tempfile.TemporaryDirectory() as directory:
        config = Path(directory) / "config.log"
        components = Path(directory) / "config_components.h"
        config.write_text(" ".join(sorted(required)) + "\n", encoding="utf-8")
        # A platform may add protocols of its own -- Windows needs udp so the
        # port's Schannel backend links -- so the fixture has to be built for
        # the platform being audited rather than from the shared list alone.
        manifest_data = json.loads(manifest.read_text(encoding="utf-8"))

        def protocol_defines(platform: str) -> str:
            names = sorted(
                set(manifest_data["protocols"]) | set(manifest_data["platforms"][platform].get("protocols", []))
            )
            text = "".join(f"#define CONFIG_{name.upper()}_PROTOCOL 1\n" for name in names)
            for key, suffix in (("bitstreamFilters", "BSF"), ("hardwareAccelerators", "HWACCEL")):
                values = set(manifest_data.get(key, [])) | set(manifest_data["platforms"][platform].get(key, []))
                text += "".join(f"#define CONFIG_{name.upper()}_{suffix} 1\n" for name in sorted(values))
            return text

        enabled_protocols = protocol_defines("webos")
        components.write_text(
            "#define CONFIG_H264_DECODER 1\n" + enabled_protocols,
            encoding="utf-8",
        )

        # The GPL branch has no platform using it any more, so exercise it
        # against a manifest that opts one in rather than letting it rot.
        gpl_manifest = Path(directory) / "ffmpeg-capabilities-gpl.json"
        gpl_data = json.loads(manifest.read_text(encoding="utf-8"))
        gpl_data["platforms"]["macos"]["gpl"] = True
        gpl_manifest.write_text(json.dumps(gpl_data), encoding="utf-8")
        gpl_flags = set(run(script, gpl_manifest, "configure", "--platform", "macos").stdout.splitlines())
        assert "--disable-gpl" in gpl_flags
        assert "--enable-gpl" in gpl_flags
        gpl_header = Path(directory) / "FfmpegCapabilitiesGpl.h"
        run(script, gpl_manifest, "cpp-header", "--platform", "macos", "--output", str(gpl_header))
        assert "inline constexpr bool kGplEnabled = true;" in gpl_header.read_text(encoding="utf-8")
        lgpl_header = Path(directory) / "FfmpegCapabilities.h"
        run(script, manifest, "cpp-header", "--platform", "macos", "--output", str(lgpl_header))
        assert "inline constexpr bool kGplEnabled = false;" in lgpl_header.read_text(encoding="utf-8")
        run(script, manifest, "audit-config", "--platform", "webos", str(config), str(components))
        windows_components = Path(directory) / "config_components_windows.h"
        windows_components.write_text("#define CONFIG_H264_DECODER 1\n" + protocol_defines("windows"), encoding="utf-8")
        run(script, manifest, "audit-components", "--platform", "windows", str(windows_components))
        without_fel = Path(directory) / "without_fel.h"
        without_fel.write_text(protocol_defines("windows").replace("#define CONFIG_DOVI_SPLIT_BSF 1\n", ""))
        missing_fel = run(script, manifest, "audit-components", "--platform", "windows", str(without_fel), expected=1)
        assert "missing bsf: dovi_split" in missing_fel.stderr
        # A build that quietly drops https keeps playing direct streams and
        # fails every transcode, so the audit has to reject it as loudly as it
        # rejects a feature nobody asked for.
        without_https = Path(directory) / "config_components_no_https.h"
        without_https.write_text(
            protocol_defines("windows").replace("#define CONFIG_HTTPS_PROTOCOL 1\n", ""), encoding="utf-8"
        )
        missing = run(
            script,
            manifest,
            "audit-components",
            "--platform",
            "windows",
            str(without_https),
            expected=1,
        )
        assert "missing protocols: https" in missing.stderr
        components.write_text("#define CONFIG_MJPEG_DECODER 1\n" + enabled_protocols, encoding="utf-8")
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
