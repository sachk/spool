#!/usr/bin/env bash
set -euo pipefail

# Use this MSYS2 installation, not Git for Windows or MinGW tools inherited from PowerShell.
export PATH="/usr/bin:$PATH"

source_dir=$(cygpath -u "$1")
headers_dir=$(cygpath -u "$2")
prefix=$(cygpath -m "$3")
build_dir=$(cygpath -u "$4")
# Microsoft's link.exe must precede MSYS's unrelated link utility.
export PATH="$(cygpath -u "$SPOOL_MSVC_BIN"):$(cygpath -u "$SPOOL_NASM_BIN"):/usr/bin:$PATH"
unset CC CXX CC_LD CXX_LD WINDRES
make -C "$headers_dir" PREFIX="$prefix" install
# MSYS pkgconf splits search paths on ':', so D:/... would become two paths.
# Keep the native prefix in .pc files for MSVC, but use POSIX paths for discovery.
export PKG_CONFIG=/usr/bin/pkgconf
export PKG_CONFIG_PATH="$(cygpath -u "$prefix/lib/pkgconfig")"
"$PKG_CONFIG" --print-errors --exists ffnvcodec
cd "$build_dir"
mapfile -t features < <(tr -d '\r' < configure-flags.txt)
"$source_dir/configure" --toolchain=msvc --arch=x86_64 --target-os=win32 \
    --prefix="$prefix" --enable-shared --disable-static \
    --pkg-config="$PKG_CONFIG" "${features[@]}"
make -j4
make install
# FFmpeg's msvc toolchain installs each import library beside its DLL, in
# bindir, while every .pc file it writes points a linker at libdir. Put them
# where they are claimed to be, or meson resolves nothing and the mpv link
# fails on a bare avcodec.lib.
cp "$(cygpath -u "$prefix")"/bin/*.lib "$(cygpath -u "$prefix")/lib/"
