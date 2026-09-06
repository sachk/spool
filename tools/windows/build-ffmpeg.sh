#!/usr/bin/env bash
set -euo pipefail

source_dir=$(cygpath -u "$1")
headers_dir=$(cygpath -u "$2")
prefix=$(cygpath -m "$3")
build_dir=$(cygpath -u "$4")
# Microsoft's link.exe must precede MSYS's unrelated link utility.
export PATH="$(cygpath -u "$SPOOL_MSVC_BIN"):$(cygpath -u "$SPOOL_NASM_BIN"):/usr/bin:$PATH"
unset CC CXX CC_LD CXX_LD WINDRES
make -C "$headers_dir" PREFIX="$prefix" install
export PKG_CONFIG_PATH="$prefix/lib/pkgconfig"
cd "$build_dir"
mapfile -t features < <(tr -d '\r' < configure-flags.txt)
"$source_dir/configure" --toolchain=msvc --arch=x86_64 --target-os=win32 \
    --prefix="$prefix" --enable-shared --disable-static \
    --pkg-config=pkgconf "${features[@]}"
make -j4
make install
