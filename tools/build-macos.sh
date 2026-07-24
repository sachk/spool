#!/usr/bin/env bash
set -euo pipefail

APP_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
# shellcheck source=tools/lib/build-common.sh
source "$APP_ROOT/tools/lib/build-common.sh"
ensure_native_shell "$APP_ROOT" "$APP_ROOT/tools/build-macos.sh" "$@"
# shellcheck source=tools/lib/qt-deploy.sh
source "$APP_ROOT/tools/lib/qt-deploy.sh"
MPV_SRC="${MPV_SRC:-$APP_ROOT/mpv}"
BUILD_ROOT="${BUILD_ROOT:-$APP_ROOT/build/macos}"
MPV_BUILD="${MPV_BUILD:-$BUILD_ROOT/mpv}"
MPV_PREFIX="${MPV_PREFIX:-$BUILD_ROOT/mpv-prefix}"
APP_BUILD="${APP_BUILD:-$BUILD_ROOT/app}"
APP_INSTALL="${APP_INSTALL:-$BUILD_ROOT/install}"
DEPLOY_APP="${DEPLOY_APP:-1}"

setup_native_ccache "$APP_ROOT"
mkdir -p "$MPV_PREFIX" "$APP_BUILD" "$APP_INSTALL"
MACOS_ICON="$BUILD_ROOT/jellyfin-native.icns"
ICONSET="$BUILD_ROOT/jellyfin-native.iconset"
rm -rf "$ICONSET"
mkdir -p "$ICONSET"
for size in 16 32 128 256 512; do
  sips -z "$size" "$size" "$APP_ROOT/app/icon.png" --out "$ICONSET/icon_${size}x${size}.png" >/dev/null
  doubled=$((size * 2))
  sips -z "$doubled" "$doubled" "$APP_ROOT/app/icon.png" --out "$ICONSET/icon_${size}x${size}@2x.png" >/dev/null
done
iconutil -c icns "$ICONSET" -o "$MACOS_ICON"
rm -rf "$ICONSET"

native_mpv_args "$MPV_PREFIX" release macos
MPV_SETUP_ARGS=(
  "${MPV_NATIVE_ARGS[@]}"
)

clean_mpv_install_prefix "$MPV_PREFIX"
mpv_meson_build "$MPV_SRC" "$MPV_BUILD" "${MPV_SETUP_ARGS[@]}"
append_colon_path PKG_CONFIG_PATH "$MPV_PREFIX/lib/pkgconfig"

cmake_build_app "$APP_ROOT" "$APP_BUILD" \
  -DCMAKE_BUILD_TYPE=Release \
  -DJELLYFIN_NATIVE_WEBOS=OFF \
  -DJELLYFIN_MACOS_ICON="$MACOS_ICON" \
  -DCMAKE_PREFIX_PATH="$MPV_PREFIX${CMAKE_PREFIX_PATH:+;$CMAKE_PREFIX_PATH}" \
  -DCMAKE_INSTALL_PREFIX="$APP_INSTALL"

if [[ "$DEPLOY_APP" == "1" ]]; then
  command -v macdeployqt >/dev/null 2>&1 || {
    echo "error: DEPLOY_APP=1 requires macdeployqt" >&2
    exit 1
  }
  [[ -d "$APP_INSTALL/jellyfin-native.app" ]] || {
    echo "error: app bundle missing at $APP_INSTALL/jellyfin-native.app" >&2
    exit 1
  }

  # Prefer Apple's /usr/bin/strip — nix's cctools-binutils strip rejects newer
  # Mach-O load commands (LC_DYLD_CHAINED_FIXUPS, cmd=0x8000001f) emitted by
  # the Apple SDK toolchain, which makes macdeployqt abort.
  if [[ -x /usr/bin/strip ]]; then
    export PATH="/usr/bin:$PATH"
  fi

  macdeployqt_shadow="$(qt_deploy_macdeployqt_shadow \
    "$APP_BUILD/build.ninja" \
    "$BUILD_ROOT/qt-tools-shadow" \
    "$(command -v macdeployqt)")"
  "$macdeployqt_shadow" "$APP_INSTALL/jellyfin-native.app" -qmldir="$APP_ROOT/qml" -no-strip
  mkdir -p "$APP_INSTALL/jellyfin-native.app/Contents/Resources/notices"
  cp -f "$APP_ROOT/app/notices/OPEN_SOURCE_NOTICES.txt" "$APP_ROOT/LICENSE" \
    "$APP_ROOT/qml/fonts/AtkinsonHyperlegible-LICENSE.txt" \
    "$APP_ROOT/qml/fonts/IBMPlexSans-LICENSE.txt" "$APP_ROOT/qml/fonts/MaterialIcons-LICENSE.txt" \
    "$APP_INSTALL/jellyfin-native.app/Contents/Resources/notices/"
  python3 "$APP_ROOT/tools/ffmpeg-capabilities.py" \
    --manifest "$APP_ROOT/tools/manifests/ffmpeg-capabilities.json" \
    audit-closure "$APP_INSTALL/jellyfin-native.app"
fi

printf '%s\n' "$APP_INSTALL/jellyfin-native.app"
