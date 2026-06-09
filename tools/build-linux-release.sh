#!/usr/bin/env bash
set -euo pipefail

APP_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
# shellcheck source=tools/lib/build-common.sh
source "$APP_ROOT/tools/lib/build-common.sh"
ensure_native_shell "$APP_ROOT" "$APP_ROOT/tools/build-linux-release.sh" "$@"
MPV_SRC="${MPV_SRC:-$APP_ROOT/mpv}"
BUILD_ROOT="${BUILD_ROOT:-$APP_ROOT/build/linux-release}"
MPV_BUILD="${MPV_BUILD:-$BUILD_ROOT/mpv}"
MPV_PREFIX="${MPV_PREFIX:-$BUILD_ROOT/mpv-prefix}"
APP_BUILD="${APP_BUILD:-$BUILD_ROOT/app}"
APP_INSTALL="${APP_INSTALL:-$BUILD_ROOT/install}"

setup_native_ccache "$APP_ROOT"
mkdir -p "$MPV_PREFIX" "$APP_BUILD" "$APP_INSTALL"

native_mpv_common_args "$MPV_PREFIX" release true
MPV_SETUP_ARGS=(
  "${MPV_NATIVE_ARGS[@]}"
  -Dwayland=enabled
  -Degl=enabled
  -Degl-wayland=enabled
  -Dgl=enabled
  -Dvulkan=enabled
  -Dvaapi=enabled
  -Dvaapi-wayland=enabled
  -Dpipewire=enabled
  -Dpulse=enabled
  -Dalsa=enabled
  -Dlibarchive=enabled
  -Dlibbluray=enabled
  -Dlua=enabled
  -Drubberband=enabled
  -Duchardet=enabled
  -Dsubrandr=disabled
)

clean_mpv_install_prefix "$MPV_PREFIX"
mpv_meson_build "$MPV_SRC" "$MPV_BUILD" "${MPV_SETUP_ARGS[@]}"

append_colon_path PKG_CONFIG_PATH "$MPV_PREFIX/lib/pkgconfig"
while IFS= read -r pc_dir; do
  append_colon_path PKG_CONFIG_PATH "$pc_dir"
done < <(find "$MPV_PREFIX/lib" -path '*/pkgconfig/mpv.pc' -exec dirname {} \;)

MPV_LIB_DIRS=("$MPV_PREFIX/lib")
while IFS= read -r lib_dir; do
  MPV_LIB_DIRS+=("$lib_dir")
done < <(find "$MPV_PREFIX/lib" -name 'libmpv.so*' -exec dirname {} \; | sort -u)
MPV_INSTALL_RPATH="$(IFS=';'; printf '%s' "${MPV_LIB_DIRS[*]}")"

cmake_build_app "$APP_ROOT" "$APP_BUILD" \
  -DCMAKE_BUILD_TYPE=Release \
  -DJELLYFIN_NATIVE_WEBOS=OFF \
  -DJELLYFIN_NATIVE_DEV_BUILD=OFF \
  -DCMAKE_PREFIX_PATH="$MPV_PREFIX${CMAKE_PREFIX_PATH:+;$CMAKE_PREFIX_PATH}" \
  -DCMAKE_INSTALL_PREFIX="$APP_INSTALL" \
  -DCMAKE_INSTALL_RPATH="$MPV_INSTALL_RPATH"

printf '%s\n' "$APP_INSTALL/bin/jellyfin-native"
