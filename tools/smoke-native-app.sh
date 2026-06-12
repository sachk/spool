#!/usr/bin/env bash
set -euo pipefail

APP_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

usage() {
  cat <<EOF
usage: $(basename "$0") [path-to-jellyfin-native-or-app-bundle]

Runs the native app startup smoke test in an isolated headless environment.
EOF
}

if [[ "${1:-}" == "-h" || "${1:-}" == "--help" ]]; then
  usage
  exit 0
fi

candidate="${1:-$APP_ROOT/build/linux-release/install/bin/jellyfin-native}"
if [[ -d "$candidate" && "$candidate" == *.app ]]; then
  candidate="$candidate/Contents/MacOS/jellyfin-native"
fi

if [[ ! -x "$candidate" ]]; then
  echo "error: executable not found: $candidate" >&2
  exit 1
fi

work="$(mktemp -d)"
cleanup() {
  rm -rf "$work"
}
trap cleanup EXIT

mkdir -p \
  "$work/home" \
  "$work/cache" \
  "$work/config" \
  "$work/data" \
  "$work/runtime" \
  "$work/diagnostics"
chmod 700 "$work/runtime"

export HOME="$work/home"
export XDG_CACHE_HOME="$work/cache"
export XDG_CONFIG_HOME="$work/config"
export XDG_DATA_HOME="$work/data"
export XDG_RUNTIME_DIR="$work/runtime"
export JELLYFIN_DIAGNOSTICS_DIR="$work/diagnostics"
export QT_QPA_PLATFORM="${JELLYFIN_SMOKE_QPA_PLATFORM:-offscreen}"
export QT_QUICK_BACKEND="${QT_QUICK_BACKEND:-software}"
export QSG_RHI_BACKEND="${QSG_RHI_BACKEND:-opengl}"
export LC_NUMERIC=C

"$candidate" --smoke-and-exit
