#!/usr/bin/env bash
set -euo pipefail

APP_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MPV_SRC="${MPV_SRC:-$APP_ROOT/mpv}"
BUILD_ROOT="${BUILD_ROOT:-$APP_ROOT/build/macos}"
MPV_BUILD="${MPV_BUILD:-$BUILD_ROOT/mpv}"
MPV_PREFIX="${MPV_PREFIX:-$BUILD_ROOT/mpv-prefix}"
APP_BUILD="${APP_BUILD:-$BUILD_ROOT/app}"
APP_INSTALL="${APP_INSTALL:-$BUILD_ROOT/install}"

export PKG_CONFIG_PATH="$MPV_PREFIX/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"

mkdir -p "$MPV_PREFIX" "$APP_BUILD" "$APP_INSTALL"

MPV_SETUP_ARGS=(
  --prefix "$MPV_PREFIX"
  --buildtype release
  --default-library shared
  -Dbuild-date=false
  -Dlibmpv=true
  -Dcplayer=true
  -Dstarfish=disabled
  -Dcoreaudio=enabled
  -Dcocoa=enabled
  -Dgl-cocoa=enabled
  -Dmacos-cocoa-cb=enabled
  -Dmacos-media-player=enabled
  -Dswift-build=enabled
  -Djavascript=disabled
  -Dlua=luajit
  -Dlibarchive=enabled
  -Dlibbluray=enabled
  -Drubberband=enabled
  -Duchardet=enabled
  -Dvulkan=disabled
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
  -DCMAKE_PREFIX_PATH="$MPV_PREFIX${CMAKE_PREFIX_PATH:+;$CMAKE_PREFIX_PATH}" \
  -DCMAKE_INSTALL_PREFIX="$APP_INSTALL"

cmake --build "$APP_BUILD" --parallel
cmake --install "$APP_BUILD"

if command -v macdeployqt >/dev/null 2>&1 && [[ -d "$APP_INSTALL/jellyfin-native.app" ]]; then
  macdeployqt "$APP_INSTALL/jellyfin-native.app" -qmldir="$APP_ROOT/qml"
fi

printf '%s\n' "$APP_INSTALL/jellyfin-native.app"
