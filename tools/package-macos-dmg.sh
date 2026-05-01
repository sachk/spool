#!/usr/bin/env bash
set -euo pipefail

APP_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_ROOT="${BUILD_ROOT:-$APP_ROOT/build/macos}"
APP_BUNDLE="${APP_BUNDLE:-$BUILD_ROOT/install/jellyfin-native.app}"
ARTIFACT_DIR="${ARTIFACT_DIR:-$APP_ROOT/dist}"
DMG_PATH="$ARTIFACT_DIR/Jellyfin-Native-macOS.dmg"

if [[ ! -d "$APP_BUNDLE" ]]; then
  echo "error: app bundle not found at $APP_BUNDLE" >&2
  exit 1
fi

mkdir -p "$ARTIFACT_DIR"
rm -f "$DMG_PATH"

if command -v create-dmg >/dev/null 2>&1; then
  create-dmg \
    --volname "Jellyfin Native" \
    --window-pos 200 120 \
    --window-size 640 420 \
    --icon-size 96 \
    --app-drop-link 460 185 \
    "$DMG_PATH" \
    "$APP_BUNDLE"
else
  hdiutil create -volname "Jellyfin Native" -srcfolder "$APP_BUNDLE" \
    -ov -format UDZO "$DMG_PATH"
fi

printf '%s\n' "$DMG_PATH"
