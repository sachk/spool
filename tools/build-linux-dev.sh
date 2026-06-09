#!/usr/bin/env bash
set -euo pipefail

APP_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
# shellcheck source=tools/lib/build-common.sh
source "$APP_ROOT/tools/lib/build-common.sh"
MPV_SRC="${MPV_SRC:-$APP_ROOT/mpv}"
BUILD_ROOT="${BUILD_ROOT:-$APP_ROOT/build/linux-dev}"
MPV_BUILD="${MPV_BUILD:-$BUILD_ROOT/mpv}"
MPV_PREFIX="${MPV_PREFIX:-$BUILD_ROOT/mpv-prefix}"
APP_BUILD="${APP_BUILD:-$BUILD_ROOT/app}"
APP_INSTALL="${APP_INSTALL:-$BUILD_ROOT/install}"
RUN_APP="${RUN_APP:-0}"

if [[ -z "${IN_NIX_SHELL:-}" ]]; then
  cd "$APP_ROOT"
  exec nix develop .#native -c bash tools/build-linux-dev.sh
fi

setup_native_ccache "$APP_ROOT"
mkdir -p "$MPV_PREFIX" "$APP_BUILD" "$APP_INSTALL"

native_mpv_common_args "$MPV_PREFIX" debugoptimized true
MPV_SETUP_ARGS=(
  "${MPV_NATIVE_ARGS[@]}"
  -Db_ndebug=false
  -Dwayland=enabled
  -Degl=enabled
  -Degl-wayland=enabled
  -Dgl=enabled
  -Dvulkan=disabled
  -Dvaapi=enabled
  -Dvaapi-wayland=enabled
  -Dpipewire=enabled
  -Dpulse=enabled
  -Dalsa=enabled
)

clean_mpv_install_prefix "$MPV_PREFIX"
mpv_meson_build "$MPV_SRC" "$MPV_BUILD" "${MPV_SETUP_ARGS[@]}"
append_colon_path PKG_CONFIG_PATH "$MPV_PREFIX/lib/pkgconfig"

cmake_build_app "$APP_ROOT" "$APP_BUILD" \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DJELLYFIN_NATIVE_WEBOS=OFF \
  -DJELLYFIN_NATIVE_DEV_BUILD=ON \
  -DCMAKE_PREFIX_PATH="$MPV_PREFIX${CMAKE_PREFIX_PATH:+;$CMAKE_PREFIX_PATH}" \
  -DCMAKE_INSTALL_PREFIX="$APP_INSTALL" \
  -DCMAKE_INSTALL_RPATH="$MPV_PREFIX/lib"

if [[ "$RUN_APP" == "1" ]]; then
  export LD_LIBRARY_PATH="$MPV_PREFIX/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
  export QT_QPA_PLATFORM="${QT_QPA_PLATFORM:-wayland}"
  exec "$APP_INSTALL/bin/jellyfin-native"
fi

printf '%s\n' "$APP_INSTALL/bin/jellyfin-native"
