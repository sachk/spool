"""Check the installed DLLs, not just FFmpeg's configured feature flags."""

import ctypes
import json
import os
from pathlib import Path
import sys

root = Path(__file__).resolve().parents[2]
prefix = Path(sys.argv[1])
pin = json.loads((root / "tools/manifests/toolchain.json").read_text())["ffmpeg"]
capabilities = json.loads((root / "tools/manifests/ffmpeg-capabilities.json").read_text())
with os.add_dll_directory(str(prefix / "bin")):
    avutil = ctypes.CDLL(str(next((prefix / "bin").glob("avutil-*.dll"))))
    avutil.av_version_info.restype = ctypes.c_char_p
    version = avutil.av_version_info().decode()
    if version != pin["version"]:
        raise RuntimeError(f"FFmpeg runtime {version} differs from shared pin {pin['version']}")
    avcodec = ctypes.CDLL(str(next((prefix / "bin").glob("avcodec-*.dll"))))
    avcodec.av_bsf_get_by_name.argtypes = [ctypes.c_char_p]
    avcodec.av_bsf_get_by_name.restype = ctypes.c_void_p
    for name in capabilities["bitstreamFilters"] + capabilities["platforms"]["windows"]["bitstreamFilters"]:
        if not avcodec.av_bsf_get_by_name(name.encode()):
            raise RuntimeError(f"FFmpeg runtime lacks required bitstream filter: {name}")
print(f"Verified Windows FFmpeg {version} and required bitstream filters")
