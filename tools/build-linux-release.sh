#!/usr/bin/env bash
set -euo pipefail

APP_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
WORKSPACE_ROOT="$(cd "$APP_ROOT/.." && pwd)"
MPV_SRC="${MPV_SRC:-$APP_ROOT/mpv}"
BUILD_ROOT="${BUILD_ROOT:-$APP_ROOT/build/linux-release}"
MPV_BUILD="${MPV_BUILD:-$BUILD_ROOT/mpv}"
MPV_PREFIX="${MPV_PREFIX:-$BUILD_ROOT/mpv-prefix}"
APP_BUILD="${APP_BUILD:-$BUILD_ROOT/app}"
APP_INSTALL="${APP_INSTALL:-$BUILD_ROOT/install}"

export CCACHE_DIR="${CCACHE_DIR:-$WORKSPACE_ROOT/.ccache}"
export CCACHE_BASEDIR="${CCACHE_BASEDIR:-$WORKSPACE_ROOT}"
export CCACHE_COMPRESS="${CCACHE_COMPRESS:-1}"
export CCACHE_SLOPPINESS="${CCACHE_SLOPPINESS:-time_macros,file_macro}"
export PKG_CONFIG_PATH="$MPV_PREFIX/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"

mkdir -p "$MPV_PREFIX" "$APP_BUILD" "$APP_INSTALL" "$CCACHE_DIR"

MPV_SETUP_ARGS=(
  --prefix "$MPV_PREFIX"
  --libdir lib
  --buildtype release
  --default-library shared
  -Db_lto=false
  -Dbuild-date=false
  -Dlibmpv=true
  -Dcplayer=true
  -Dstarfish=disabled
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
)

if [[ -f "$MPV_BUILD/build.ninja" ]]; then
  meson setup --reconfigure "$MPV_BUILD" "$MPV_SRC" "${MPV_SETUP_ARGS[@]}"
else
  meson setup "$MPV_BUILD" "$MPV_SRC" "${MPV_SETUP_ARGS[@]}"
fi
meson compile -C "$MPV_BUILD"
meson install -C "$MPV_BUILD"

cmake -S "$APP_ROOT" -B "$APP_BUILD" -GNinja \
  -DCMAKE_BUILD_TYPE=Release \
  -DJELLYFIN_NATIVE_WEBOS=OFF \
  -DJELLYFIN_NATIVE_DEV_BUILD=OFF \
  -DCMAKE_PREFIX_PATH="$MPV_PREFIX${CMAKE_PREFIX_PATH:+;$CMAKE_PREFIX_PATH}" \
  -DCMAKE_INSTALL_PREFIX="$APP_INSTALL" \
  -DCMAKE_INSTALL_RPATH="$MPV_PREFIX/lib"

cmake --build "$APP_BUILD" --parallel
cmake --install "$APP_BUILD"

printf '%s\n' "$APP_INSTALL/bin/jellyfin-native"
