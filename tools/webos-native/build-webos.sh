#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
# shellcheck source=tools/lib/manifest-sources.sh
source "$ROOT/tools/lib/manifest-sources.sh"
MANIFEST="${WEBOS_THIRD_PARTY_MANIFEST:-$ROOT/tools/manifests/webos-third-party.json}"
PHASE="${1:-all}"
SDK_ROOT="${WEBOS_SDK_ROOT:-$ROOT/build/webos-sdk/arm-webos-linux-gnueabi_sdk-buildroot}"

case "$PHASE" in
  all|qt-target|dependencies|app|stage)
    WEBOS_SDK_ROOT="$SDK_ROOT" "$ROOT/tools/webos-native/restore-toolchain.sh"
    ;;
esac


fetch_archive() {
  local source_name="$1"
  local filename="$2"
  local url sha
  url="$(manifest_source_field "$MANIFEST" "$source_name" url)"
  sha="$(manifest_source_field "$MANIFEST" "$source_name" sha256)"
  download_verified "$url" "$sha" "$ROOT/build/downloads/$filename"
}

run_phase() {
  local phase="$1"
  case "$phase" in
    fetch)
      "$ROOT/tools/webos-native/build-qt6-611.sh" fetch
      "$ROOT/tools/webos-native/build-third-party.sh" fetch
      "$ROOT/tools/webos-native/build-curl.sh" fetch
      "$ROOT/tools/webos-native/build-qcoro.sh" fetch
      fetch_archive ffmpeg ffmpeg-8.1.2.tar.xz
      ;;
    qt-host)
      "$ROOT/tools/webos-native/build-qt6-611.sh" host
      ;;
    qt-target)
      QT_STATIC="${QT_STATIC:-1}" "$ROOT/tools/webos-native/build-qt6-611.sh" target
      ;;
    dependencies)
      "$ROOT/tools/webos-native/build-third-party.sh" build
      "$ROOT/tools/webos-native/build-curl.sh" build
      QT_STATIC="${QT_STATIC:-1}" "$ROOT/tools/webos-native/build-qcoro.sh" build
      "$ROOT/tools/webos-native/build-ffmpeg.sh"
      ;;
    app|stage|package)
      "$ROOT/build-ipk.sh" "$phase"
      ;;
    *)
      echo "unknown webOS build phase: $phase" >&2
      exit 2
      ;;
  esac
}

if [[ "$PHASE" == "all" ]]; then
  for phase in fetch qt-host qt-target dependencies app stage package; do
    echo
    echo ">>> webOS phase: $phase"
    run_phase "$phase"
  done
else
  run_phase "$PHASE"
fi
