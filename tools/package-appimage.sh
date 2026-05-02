#!/usr/bin/env bash
set -euo pipefail

APP_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_ROOT="${BUILD_ROOT:-$APP_ROOT/build/linux-release/install/bin}"
MPV_PREFIX="${MPV_PREFIX:-$APP_ROOT/build/linux-release/mpv-prefix}"
APPDIR="${APPDIR:-$APP_ROOT/build/appimage/AppDir}"
ARTIFACT_DIR="${ARTIFACT_DIR:-$APP_ROOT/dist}"
LINUXDEPLOY="${LINUXDEPLOY:-$APP_ROOT/build/appimage/linuxdeploy-x86_64.AppImage}"
QT_PLUGIN="${QT_PLUGIN:-$APP_ROOT/build/appimage/linuxdeploy-plugin-qt-x86_64.AppImage}"

if [[ ! -x "$BUILD_ROOT/jellyfin-native" ]]; then
  echo "error: build output not found at $BUILD_ROOT/jellyfin-native" >&2
  exit 1
fi

mkdir -p "$APPDIR/usr/bin" "$APPDIR/usr/lib" "$APPDIR/usr/share/applications" \
  "$APPDIR/usr/share/icons/hicolor/256x256/apps" "$ARTIFACT_DIR" "$(dirname "$LINUXDEPLOY")"
rm -rf "$APPDIR/usr/bin" "$APPDIR/usr/lib" "$APPDIR/usr/share/applications" \
  "$APPDIR/usr/share/icons/hicolor/256x256/apps"
mkdir -p "$APPDIR/usr/bin" "$APPDIR/usr/lib" "$APPDIR/usr/share/applications" \
  "$APPDIR/usr/share/icons/hicolor/256x256/apps"

cp -f "$BUILD_ROOT/jellyfin-native" "$APPDIR/usr/bin/jellyfin-native"
if compgen -G "$MPV_PREFIX/lib/libmpv.so*" >/dev/null; then
  cp -a "$MPV_PREFIX"/lib/libmpv.so* "$APPDIR/usr/lib/"
fi
cp -f "$APP_ROOT/app/icon.png" "$APPDIR/usr/share/icons/hicolor/256x256/apps/jellyfin-native.png"

cat > "$APPDIR/usr/share/applications/jellyfin-native.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=Jellyfin Native
Exec=jellyfin-native
Icon=jellyfin-native
Categories=AudioVideo;Video;
DESKTOP

if [[ ! -x "$LINUXDEPLOY" ]]; then
  curl -L --fail -o "$LINUXDEPLOY" \
    https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage
  chmod +x "$LINUXDEPLOY"
fi
if [[ ! -x "$QT_PLUGIN" ]]; then
  curl -L --fail -o "$QT_PLUGIN" \
    https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage
  chmod +x "$QT_PLUGIN"
fi

export LD_LIBRARY_PATH="$MPV_PREFIX/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export EXTRA_QT_PLUGINS="${EXTRA_QT_PLUGINS:-wayland-shell-integration/libxdg-shell.so;wayland-graphics-integration-client/libqt-plugin-wayland-egl.so}"
export QML_SOURCES_PATHS="${QML_SOURCES_PATHS:-$APP_ROOT/qml}"
export OUTPUT="${OUTPUT:-Jellyfin-Native-x86_64.AppImage}"

"$LINUXDEPLOY" --appdir "$APPDIR" --plugin qt --output appimage
mv -f "$OUTPUT" "$ARTIFACT_DIR/$OUTPUT"
printf '%s\n' "$ARTIFACT_DIR/$OUTPUT"
