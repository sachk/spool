#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
# shellcheck source=tools/lib/manifest-sources.sh
source "$ROOT/tools/lib/manifest-sources.sh"
MANIFEST="${WEBOS_THIRD_PARTY_MANIFEST:-$ROOT/tools/manifests/webos-third-party.json}"
SDK_ROOT="${WEBOS_SDK_ROOT:-$ROOT/build/webos-sdk/arm-webos-linux-gnueabi_sdk-buildroot}"
SDK_BIN="$SDK_ROOT/bin"
SYSROOT="$SDK_ROOT/arm-webos-linux-gnueabi/sysroot"
TARGET_PREFIX="${WEBOS_TARGET_PREFIX:-/usr/local/webos-native}"
PREFIX="${WEBOS_NATIVE_PREFIX:-$SYSROOT$TARGET_PREFIX}"
LUA_VERSION="${LUA_VERSION:-$(manifest_source_field "$MANIFEST" lua version)}"
LUA_URL="${LUA_URL:-$(manifest_source_field "$MANIFEST" lua url)}"
LUA_SHA256="${LUA_SHA256:-$(manifest_source_field "$MANIFEST" lua sha256)}"
ARCHIVE="${LUA_ARCHIVE:-$ROOT/build/downloads/lua-${LUA_VERSION}.tar.gz}"
SRC_DIR="${LUA_SRC_DIR:-$ROOT/build/lua-src}"
BUILD_DIR="${LUA_BUILD_DIR:-$ROOT/build/lua-build}"

mkdir -p "$ROOT/build" "$PREFIX/include" "$PREFIX/lib/pkgconfig"

download_verified "$LUA_URL" "$LUA_SHA256" "$ARCHIVE"

rm -rf "$SRC_DIR" "$BUILD_DIR"
mkdir -p "$SRC_DIR" "$BUILD_DIR"
tar -xf "$ARCHIVE" -C "$SRC_DIR" --strip-components=1

export CC="$SDK_BIN/arm-webos-linux-gnueabi-gcc"
export AR="$SDK_BIN/arm-webos-linux-gnueabi-ar rcu"
export RANLIB="$SDK_BIN/arm-webos-linux-gnueabi-ranlib"
export STRIP="$SDK_BIN/arm-webos-linux-gnueabi-strip"

cd "$SRC_DIR"

make clean >/dev/null 2>&1 || true
make linux \
  CC="$CC --sysroot=$SYSROOT" \
  AR="$AR" \
  RANLIB="$RANLIB" \
  MYCFLAGS="-fPIC -O2 -DLUA_USE_POSIX" \
  MYLDFLAGS="--sysroot=$SYSROOT"

"$SDK_BIN/arm-webos-linux-gnueabi-gcc" \
  --sysroot="$SYSROOT" \
  -shared \
  -Wl,-soname,liblua5.2.so.0 \
  -o "$BUILD_DIR/liblua5.2.so.0.0.0" \
  -Wl,--whole-archive src/liblua.a -Wl,--no-whole-archive \
  -lm -ldl

ln -sf liblua5.2.so.0.0.0 "$BUILD_DIR/liblua5.2.so.0"
ln -sf liblua5.2.so.0 "$BUILD_DIR/liblua5.2.so"

install -m 0755 "$BUILD_DIR/liblua5.2.so.0.0.0" "$PREFIX/lib/liblua5.2.so.0.0.0"
ln -sf liblua5.2.so.0.0.0 "$PREFIX/lib/liblua5.2.so.0"
ln -sf liblua5.2.so.0 "$PREFIX/lib/liblua5.2.so"

install -m 0644 src/lua.h "$PREFIX/include/lua.h"
install -m 0644 src/luaconf.h "$PREFIX/include/luaconf.h"
install -m 0644 src/lualib.h "$PREFIX/include/lualib.h"
install -m 0644 src/lauxlib.h "$PREFIX/include/lauxlib.h"
install -m 0644 src/lua.hpp "$PREFIX/include/lua.hpp"

cat >"$PREFIX/lib/pkgconfig/lua.pc" <<EOF
prefix=$TARGET_PREFIX
exec_prefix=\${prefix}
libdir=\${exec_prefix}/lib
includedir=\${prefix}/include

Name: Lua
Description: Embedded language runtime
Version: $LUA_VERSION
Libs: -L\${libdir} -llua5.2 -lm -ldl
Cflags: -I\${includedir}
EOF

cat >"$PREFIX/lib/pkgconfig/lua52.pc" <<EOF
prefix=$TARGET_PREFIX
exec_prefix=\${prefix}
libdir=\${exec_prefix}/lib
includedir=\${prefix}/include

Name: Lua 5.2
Description: Embedded language runtime
Version: $LUA_VERSION
Libs: -L\${libdir} -llua5.2 -lm -ldl
Cflags: -I\${includedir}
EOF

printf '\nInstalled Lua %s into %s\n' "$LUA_VERSION" "$PREFIX"
