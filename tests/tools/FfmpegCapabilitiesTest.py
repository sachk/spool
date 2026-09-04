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

    meson = set(run(script, manifest, "meson", "--platform", "windows").stdout.splitlines())
    assert {"ffmpeg:gpl=disabled", "ffmpeg:version3=disabled", "ffmpeg:nonfree=disabled"} <= meson
    # Windows takes FFmpeg as a Meson subproject rather than through configure,
    # so the network switches its platform entry carries have to survive the
    # translation or its HLS playback silently loses every transcode.
    assert {"ffmpeg:network=enabled", "ffmpeg:schannel=enabled"} <= meson
    assert {"ffmpeg:https_protocol=enabled", "ffmpeg:tls_protocol=enabled"} <= meson
    assert "ffmpeg:mjpeg_decoder=enabled" not in meson

    # Meson has no wildcard and auto_features cannot be scoped to a
    # subproject, so anything the port declares and the manifest omits has to
    # be refused by name or it is built.
    with tempfile.TemporaryDirectory() as options_directory:
        options = Path(options_directory) / "meson_options.txt"
        options.write_text(
            "option('hls_demuxer', type: 'feature', value: 'auto')\n"
            "option('mjpeg_decoder', type: 'feature', value: 'auto')\n"
            "option('https_protocol', type: 'feature', value: 'auto')\n"
            "option('gopher_protocol', type: 'feature', value: 'auto')\n"
            "option('asrc_abuffer_filter', type: 'feature', value: 'auto')\n"
            "option('showinfo_filter', type: 'feature', value: 'auto')\n"
            "option('gpl', type: 'feature', value: 'disabled')\n",
            encoding="utf-8",
        )
        scoped = set(
            run(
                script, manifest, "meson", "--platform", "windows", "--component-options", str(options)
            ).stdout.splitlines()
        )
        assert "ffmpeg:mjpeg_decoder=disabled" in scoped
        assert "ffmpeg:gopher_protocol=disabled" in scoped
        assert "ffmpeg:showinfo_filter=disabled" in scoped
        assert "ffmpeg:hls_demuxer=enabled" in scoped
        assert "ffmpeg:https_protocol=enabled" in scoped
        # The manifest names this filter abuffer; the port calls the option
        # asrc_abuffer, and disabling it by the wrong name would silently drop
        # the audio graph's source.
        assert "ffmpeg:asrc_abuffer_filter=disabled" not in scoped
        # An option that is not a component must be left alone entirely.
        assert not any(flag.startswith("ffmpeg:gpl=") and flag.endswith("disabled") for flag in scoped - meson)
        for name in ("mjpeg_decoder", "gopher_protocol", "showinfo_filter"):
            assert f"ffmpeg:{name}=enabled" not in scoped

        # Windows will not start a process with thousands of -D arguments, so
        # the same settings have to reach Meson as a native file instead.
        native = Path(options_directory) / "ffmpeg-features.ini"
        run(
            script, manifest, "meson", "--platform", "windows",
            "--component-options", str(options), "--native-file", str(native),
        )
        text = native.read_text(encoding="utf-8")
        assert "[ffmpeg:project options]" in text
        assert "[ffmpeg:built-in options]" in text
        assert "mjpeg_decoder = 'disabled'" in text
        assert "https_protocol = 'enabled'" in text
        # default_library is a Meson built-in, not one of the port's options,
        # and belongs in the other section or Meson rejects the file.
        builtin, _, project = text.partition("[ffmpeg:project options]")
        assert "default_library = 'static'" in builtin
        assert "default_library" not in project
        # auto_features is a core option Meson will not scope to a subproject;
        # carrying it would only reassert the thing being worked around.
        assert "auto_features" not in text
    assert "ffmpeg:png_decoder=enabled" not in meson
    assert "ffmpeg:webp_decoder=enabled" not in meson

    with tempfile.TemporaryDirectory() as directory:
        config = Path(directory) / "config.log"
        components = Path(directory) / "config_components.h"
        config.write_text(" ".join(sorted(required)) + "\n", encoding="utf-8")
        enabled_protocols = "".join(
            f"#define CONFIG_{name.upper()}_PROTOCOL 1\n"
            for name in json.loads(manifest.read_text(encoding="utf-8"))["protocols"]
        )
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
        run(script, manifest, "audit-components", "--platform", "windows", str(components))
        # A build that quietly drops https keeps playing direct streams and
        # fails every transcode, so the audit has to reject it as loudly as it
        # rejects a feature nobody asked for.
        without_https = Path(directory) / "config_components_no_https.h"
        without_https.write_text(
            enabled_protocols.replace("#define CONFIG_HTTPS_PROTOCOL 1\n", ""), encoding="utf-8"
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
