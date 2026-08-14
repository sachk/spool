#!/usr/bin/env bash
set -euo pipefail

APP_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
# shellcheck source=tools/lib/build-common.sh
source "$APP_ROOT/tools/lib/build-common.sh"
ensure_native_shell "$APP_ROOT" "$APP_ROOT/tools/package-macos-dmg.sh" "$@"
APP_VERSION="$(read_project_version "$APP_ROOT")"
BUILD_ROOT="${BUILD_ROOT:-$APP_ROOT/build/macos}"
APP_BUNDLE="${APP_BUNDLE:-$BUILD_ROOT/install/jellyfin-native.app}"
ARTIFACT_DIR="${ARTIFACT_DIR:-$APP_ROOT/dist}"

if [[ ! -d "$APP_BUNDLE" ]]; then
  echo "error: app bundle not found at $APP_BUNDLE" >&2
  exit 1
fi

# Apple Silicon and Intel DMGs ship side by side in one release, so name each
# for the architecture its binary actually carries rather than for the host
# that happened to build it.
APP_BINARY="$APP_BUNDLE/Contents/MacOS/jellyfin-native"
if [[ -z "${APP_ARCH:-}" ]]; then
  APP_ARCH="$(lipo -archs "$APP_BINARY" 2>/dev/null | tr ' ' '-')"
fi
if [[ -z "$APP_ARCH" ]]; then
  echo "error: could not determine the architecture of $APP_BINARY" >&2
  exit 1
fi
DMG_PATH="$ARTIFACT_DIR/Spool-for-Jellyfin-${APP_VERSION}-macOS-${APP_ARCH}.dmg"

mkdir -p "$ARTIFACT_DIR"
rm -f "$DMG_PATH"

command -v create-dmg >/dev/null 2>&1 || {
  echo "error: create-dmg is required for macOS release packaging" >&2
  exit 1
}
create_dmg_args=(
  --volname "Spool for Jellyfin"
  --window-pos 200 120
  --window-size 640 420
  --icon-size 96
  --app-drop-link 460 185
)
if [[ "${CI:-}" == true ]]; then
  # Finder can retain the temporary image until hdiutil's two-minute detach
  # timeout expires on hosted runners. CI needs a reliable image, not Finder
  # window metadata.
  create_dmg_args+=(--skip-jenkins)
fi
create-dmg "${create_dmg_args[@]}" "$DMG_PATH" "$APP_BUNDLE"

printf '%s\n' "$DMG_PATH"
