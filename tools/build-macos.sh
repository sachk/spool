#!/usr/bin/env bash
set -euo pipefail

APP_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
# shellcheck source=tools/lib/build-common.sh
source "$APP_ROOT/tools/lib/build-common.sh"
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
  --libdir lib
  --buildtype release
  --default-library shared
  -Dbuild-date=false
  -Dlibmpv=true
  -Dcplayer=false
  -Dstarfish=disabled
  -Dcoreaudio=enabled
  -Dcocoa=disabled
  -Dgl-cocoa=disabled
  -Dmacos-cocoa-cb=disabled
  -Dmacos-media-player=disabled
  -Dswift-build=disabled
  -Djavascript=disabled
  -Dlua=luajit
  -Dlibarchive=enabled
  -Dlibbluray=enabled
  -Drubberband=enabled
  -Duchardet=enabled
  -Dvulkan=disabled
)

mpv_meson_build "$MPV_SRC" "$MPV_BUILD" "${MPV_SETUP_ARGS[@]}"

cmake_build_app "$APP_ROOT" "$APP_BUILD" \
  -DCMAKE_BUILD_TYPE=Release \
  -DJELLYFIN_NATIVE_WEBOS=OFF \
  -DCMAKE_PREFIX_PATH="$MPV_PREFIX${CMAKE_PREFIX_PATH:+;$CMAKE_PREFIX_PATH}" \
  -DCMAKE_INSTALL_PREFIX="$APP_INSTALL"

if command -v macdeployqt >/dev/null 2>&1 && [[ -d "$APP_INSTALL/jellyfin-native.app" ]]; then
  # Prefer Apple's /usr/bin/strip — nix's cctools-binutils strip rejects newer
  # Mach-O load commands (LC_DYLD_CHAINED_FIXUPS, cmd=0x8000001f) emitted by
  # the Apple SDK toolchain, which makes macdeployqt abort.
  if [[ -x /usr/bin/strip ]]; then
    export PATH="/usr/bin:$PATH"
  fi

  # Nix splits Qt tools across outputs. macdeployqt asks QLibraryInfo for
  # QT_HOST_LIBEXECS/QT_INSTALL_LIBEXECS, which can be empty or point at a
  # read-only qtbase path that lacks qtdeclarative's qmlimportscanner. Run a
  # copied macdeployqt with a local qt.conf so that libexec resolves to our
  # writable tool shadow containing qmlimportscanner.
  qmlscanner="$(resolve_qmlimportscanner "$APP_BUILD/build.ninja")"
  if [[ -z "$qmlscanner" ]]; then
    printf 'error: qmlimportscanner is required for macdeployqt QML deployment\n' >&2
    exit 1
  fi

  qt_shadow="$BUILD_ROOT/qt-tools-shadow"
  qt_shadow_bin="$qt_shadow/bin"
  qt_shadow_libexec="$qt_shadow/libexec"
  mkdir -p "$qt_shadow_bin" "$qt_shadow_libexec"
  cp "$(command -v macdeployqt)" "$qt_shadow_bin/macdeployqt"
  chmod +x "$qt_shadow_bin/macdeployqt"
  ln -sf "$qmlscanner" "$qt_shadow_libexec/qmlimportscanner"

  qt_prefix=""
  qt_plugins=""
  qt_qml=""
  if command -v qtpaths6 >/dev/null 2>&1; then
    qt_prefix="$(qtpaths6 -query QT_INSTALL_PREFIX 2>/dev/null || true)"
    qt_plugins="$(qtpaths6 -query QT_INSTALL_PLUGINS 2>/dev/null || true)"
    qt_qml="$(qtpaths6 -query QT_INSTALL_QML 2>/dev/null || true)"
  elif command -v qtpaths >/dev/null 2>&1; then
    qt_prefix="$(qtpaths -query QT_INSTALL_PREFIX 2>/dev/null || true)"
    qt_plugins="$(qtpaths -query QT_INSTALL_PLUGINS 2>/dev/null || true)"
    qt_qml="$(qtpaths -query QT_INSTALL_QML 2>/dev/null || true)"
  fi

  cat >"$qt_shadow_bin/qt.conf" <<EOF
[Paths]
Prefix=${qt_prefix:-$qt_shadow}
HostPrefix=$qt_shadow
HostLibraryExecutables=$qt_shadow_libexec
LibraryExecutables=$qt_shadow_libexec
Plugins=${qt_plugins:-}
QmlImports=${qt_qml:-}
EOF

  "$qt_shadow_bin/macdeployqt" "$APP_INSTALL/jellyfin-native.app" -qmldir="$APP_ROOT/qml" -no-strip
fi

printf '%s\n' "$APP_INSTALL/jellyfin-native.app"
