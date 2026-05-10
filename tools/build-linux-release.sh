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
append_pkg_config_path() {
  local dir="$1"
  [[ -d "$dir" ]] || return 0
  case ":${PKG_CONFIG_PATH:-}:" in
    *":$dir:"*) ;;
    *) export PKG_CONFIG_PATH="$dir${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}" ;;
  esac
}

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
  -Dsubrandr=disabled
)

if [[ -f "$MPV_BUILD/meson-info/meson-info.json" ]]; then
  cached_src="$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1])).get("directories",{}).get("source",""))' "$MPV_BUILD/meson-info/meson-info.json" 2>/dev/null || true)"
  if [[ -n "$cached_src" && "$cached_src" != "$MPV_SRC" ]]; then
    echo "mpv build dir cached with stale source path ($cached_src != $MPV_SRC); wiping" >&2
    rm -rf "$MPV_BUILD"
  fi
fi
if [[ -f "$MPV_BUILD/build.ninja" ]]; then
  meson setup --reconfigure "$MPV_BUILD" "$MPV_SRC" "${MPV_SETUP_ARGS[@]}"
else
  meson setup "$MPV_BUILD" "$MPV_SRC" "${MPV_SETUP_ARGS[@]}"
fi
meson compile -C "$MPV_BUILD"
meson install -C "$MPV_BUILD"

append_pkg_config_path "$MPV_PREFIX/lib/pkgconfig"
while IFS= read -r pc_dir; do
  append_pkg_config_path "$pc_dir"
done < <(find "$MPV_PREFIX/lib" -path '*/pkgconfig/mpv.pc' -exec dirname {} \;)

MPV_LIB_DIRS=("$MPV_PREFIX/lib")
while IFS= read -r lib_dir; do
  MPV_LIB_DIRS+=("$lib_dir")
done < <(find "$MPV_PREFIX/lib" -name 'libmpv.so*' -exec dirname {} \; | sort -u)
MPV_INSTALL_RPATH="$(IFS=';'; printf '%s' "${MPV_LIB_DIRS[*]}")"

if [[ -f "$APP_BUILD/CMakeCache.txt" ]] && ! grep -q "^CMAKE_HOME_DIRECTORY:INTERNAL=$APP_ROOT$" "$APP_BUILD/CMakeCache.txt"; then
  echo "app build dir cached with stale source path; wiping" >&2
  rm -rf "$APP_BUILD"
  mkdir -p "$APP_BUILD"
fi

cmake -S "$APP_ROOT" -B "$APP_BUILD" -GNinja \
  -DCMAKE_BUILD_TYPE=Release \
  -DJELLYFIN_NATIVE_WEBOS=OFF \
  -DJELLYFIN_NATIVE_DEV_BUILD=OFF \
  -DCMAKE_PREFIX_PATH="$MPV_PREFIX${CMAKE_PREFIX_PATH:+;$CMAKE_PREFIX_PATH}" \
  -DCMAKE_INSTALL_PREFIX="$APP_INSTALL" \
  -DCMAKE_INSTALL_RPATH="$MPV_INSTALL_RPATH"

cmake --build "$APP_BUILD" --parallel
cmake --install "$APP_BUILD"

printf '%s\n' "$APP_INSTALL/bin/jellyfin-native"
