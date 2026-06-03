#!/usr/bin/env bash
set -euo pipefail

APP_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
WORKSPACE_ROOT="$(cd "$APP_ROOT/.." && pwd)"
MPV_SRC="${MPV_SRC:-$APP_ROOT/mpv}"
MPV_BUILD="${MPV_BUILD:-$MPV_SRC/build/linux-dev}"
MPV_PREFIX="${MPV_PREFIX:-$WORKSPACE_ROOT/build/linux-dev/mpv-prefix}"
APP_BUILD="${APP_BUILD:-$APP_ROOT/build-linux-dev}"
RUN_APP="${RUN_APP:-0}"

if [[ -z "${IN_NIX_SHELL:-}" ]]; then
  cd "$WORKSPACE_ROOT"
  exec nix-shell --run "cd jellyfin-webos && bash tools/build-linux-dev.sh"
fi

export CCACHE_DIR="${CCACHE_DIR:-$WORKSPACE_ROOT/.ccache}"
export CCACHE_BASEDIR="${CCACHE_BASEDIR:-$WORKSPACE_ROOT}"
export CCACHE_COMPRESS="${CCACHE_COMPRESS:-1}"
export CCACHE_SLOPPINESS="${CCACHE_SLOPPINESS:-time_macros,file_macro}"
export PKG_CONFIG_PATH="$MPV_PREFIX/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"

mkdir -p "$MPV_PREFIX" "$APP_BUILD" "$CCACHE_DIR"

MPV_SETUP_ARGS=(
  --prefix "$MPV_PREFIX"
  --buildtype debugoptimized
  --default-library shared
  -Db_lto=false
  -Db_ndebug=false
  -Dbuild-date=false
  -Dlibmpv=true
  -Dcplayer=true
  # starfish: webOS-only VO patch; the option does not exist in upstream mpv,
  # so it must not be passed on native Linux/macOS builds.
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

if [[ -f "$MPV_BUILD/build.ninja" ]]; then
  meson setup --reconfigure "$MPV_BUILD" "$MPV_SRC" "${MPV_SETUP_ARGS[@]}"
else
  meson setup "$MPV_BUILD" "$MPV_SRC" "${MPV_SETUP_ARGS[@]}"
fi
meson compile -C "$MPV_BUILD"
meson install -C "$MPV_BUILD"

cmake -S "$APP_ROOT" -B "$APP_BUILD" -GNinja \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DJELLYFIN_NATIVE_WEBOS=OFF \
  -DJELLYFIN_NATIVE_DEV_BUILD=ON \
  -DCMAKE_PREFIX_PATH="$MPV_PREFIX${CMAKE_PREFIX_PATH:+;$CMAKE_PREFIX_PATH}" \
  -DCMAKE_INSTALL_RPATH="$MPV_PREFIX/lib"

cmake --build "$APP_BUILD" --parallel

if [[ "$RUN_APP" == "1" ]]; then
  export LD_LIBRARY_PATH="$MPV_PREFIX/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
  export QT_QPA_PLATFORM="${QT_QPA_PLATFORM:-wayland}"
  exec "$APP_BUILD/jellyfin-native"
fi
