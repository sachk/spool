#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
SDK_ROOT="${WEBOS_SDK_ROOT:-$ROOT/build/webos-sdk/arm-webos-linux-gnueabi_sdk-buildroot}"
SDK_BIN="$SDK_ROOT/bin"
SYSROOT="$SDK_ROOT/arm-webos-linux-gnueabi/sysroot"
TARGET_PREFIX="${WEBOS_TARGET_PREFIX:-/usr/local/webos-native}"
PREFIX="${WEBOS_NATIVE_PREFIX:-$SYSROOT$TARGET_PREFIX}"
SRC_ROOT="${WEBOS_NATIVE_SRC_ROOT:-$ROOT/build/third_party}"
BUILD_ROOT="${WEBOS_NATIVE_BUILD_ROOT:-$ROOT/build/webos-thirdparty-build}"
CROSS_FILE="$BUILD_ROOT/webos.cross.ini"

mkdir -p "$SRC_ROOT" "$BUILD_ROOT" "$PREFIX"

if [[ ! -x "$SDK_BIN/arm-webos-linux-gnueabi-gcc" ]]; then
  printf 'Missing webOS SDK compiler under %s\n' "$SDK_BIN" >&2
  exit 1
fi

clone_if_missing() {
  local dir="$1"
  local url="$2"
  if [[ ! -d "$SRC_ROOT/$dir/.git" && ! -f "$SRC_ROOT/$dir/meson.build" ]]; then
    git clone --depth 1 "$url" "$SRC_ROOT/$dir"
  fi
}

clone_if_missing fribidi https://github.com/fribidi/fribidi.git
clone_if_missing harfbuzz https://github.com/harfbuzz/harfbuzz.git
clone_if_missing libass https://github.com/libass/libass.git
clone_if_missing libplacebo https://github.com/haasn/libplacebo.git

if [[ -f "$SRC_ROOT/libplacebo/.gitmodules" ]]; then
  git -C "$SRC_ROOT/libplacebo" submodule update --init --depth 1 \
    3rdparty/fast_float \
    3rdparty/glad \
    3rdparty/jinja \
    3rdparty/markupsafe \
    3rdparty/Vulkan-Headers
fi

cat >"$CROSS_FILE" <<EOF
[binaries]
c = '$SDK_BIN/arm-webos-linux-gnueabi-gcc'
cpp = '$SDK_BIN/arm-webos-linux-gnueabi-g++'
ar = '$SDK_BIN/arm-webos-linux-gnueabi-ar'
strip = '$SDK_BIN/arm-webos-linux-gnueabi-strip'
ranlib = '$SDK_BIN/arm-webos-linux-gnueabi-ranlib'
pkg-config = '$SDK_BIN/pkg-config'

[properties]
sys_root = '$SYSROOT'
pkg_config_libdir = ['$PREFIX/lib/pkgconfig', '$SYSROOT/usr/lib/pkgconfig', '$SYSROOT/usr/share/pkgconfig']
needs_exe_wrapper = true

[host_machine]
system = 'linux'
cpu_family = 'arm'
cpu = 'armv7'
endian = 'little'
EOF

export PKG_CONFIG_PATH="$PREFIX/lib/pkgconfig"
export PKG_CONFIG_LIBDIR="$PREFIX/lib/pkgconfig:$SYSROOT/usr/lib/pkgconfig:$SYSROOT/usr/share/pkgconfig"
export PKG_CONFIG_SYSROOT_DIR="$SYSROOT"

build_meson() {
  local name="$1"
  local src_dir="$2"
  shift 2
  local build_dir="$BUILD_ROOT/$name"

  rm -rf "$build_dir"
  meson setup "$build_dir" "$src_dir" \
    --cross-file "$CROSS_FILE" \
    --prefix "$TARGET_PREFIX" \
    "$@"
  meson compile -C "$build_dir"
  DESTDIR="$SYSROOT" meson install -C "$build_dir"
}

build_meson fribidi "$SRC_ROOT/fribidi" \
  --default-library static \
  -Ddocs=false \
  -Dbin=false \
  -Dtests=false \
  --buildtype release

build_meson harfbuzz "$SRC_ROOT/harfbuzz" \
  --default-library static \
  -Dtests=disabled \
  -Dutilities=disabled \
  -Ddocs=disabled \
  -Dglib=disabled \
  -Dgobject=disabled \
  -Dcairo=disabled \
  -Dchafa=disabled \
  -Dpng=disabled \
  -Dzlib=disabled \
  -Dicu=disabled \
  -Dgraphite=disabled \
  -Dgraphite2=disabled \
  -Dfreetype=disabled \
  -Dfontations=disabled \
  -Dgdi=disabled \
  -Ddirectwrite=disabled \
  -Dcoretext=disabled \
  -Dharfrust=disabled \
  -Dkbts=disabled \
  -Dwasm=disabled \
  -Draster=disabled \
  -Dvector=disabled \
  -Dgpu=disabled \
  -Dsubset=disabled \
  --buildtype release

build_meson libass "$SRC_ROOT/libass" \
  --default-library static \
  -Dtest=disabled \
  -Dcompare=disabled \
  -Dprofile=disabled \
  -Dfuzz=disabled \
  -Dcheckasm=disabled \
  -Dfontconfig=enabled \
  -Dlibunibreak=disabled \
  -Dasm=disabled \
  --buildtype release

build_meson libplacebo "$SRC_ROOT/libplacebo" \
  --default-library static \
  -Ddemos=false \
  -Dtests=false \
  -Dbench=false \
  -Dfuzz=false \
  -Dvulkan=disabled \
  -Dvk-proc-addr=disabled \
  -Dd3d11=disabled \
  -Dshaderc=disabled \
  -Dglslang=disabled \
  -Dlcms=disabled \
  -Ddovi=disabled \
  -Dlibdovi=disabled \
  -Dxxhash=disabled \
  -Dopengl=enabled \
  -Dgl-proc-addr=enabled \
  --buildtype release

printf '\nInstalled target dependencies into %s\n' "$PREFIX"
