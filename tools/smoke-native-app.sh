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

bundled_app=0
candidate="${1:-$APP_ROOT/build/linux-release/install/bin/jellyfin-native}"
if [[ -d "$candidate" && "$candidate" == *.app ]]; then
  bundled_app=1
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

join_colon_paths() {
  local IFS=:
  printf '%s' "$*"
}

filter_qt_version_paths() {
  local input="$1"
  local version="$2"
  local path
  local paths=()
  IFS=: read -r -a paths <<< "$input"
  local filtered=()
  for path in "${paths[@]}"; do
    [[ -d "$path" ]] || continue
    [[ "$path" == *"-${version}/"* || "$path" == *"-${version}" ]] || continue
    filtered+=("$path")
  done
  join_colon_paths "${filtered[@]}"
}

qt_version_subdirs_from_roots() {
  local roots_input="$1"
  local version="$2"
  local suffix="$3"
  local root
  local roots=()
  IFS=: read -r -a roots <<< "$roots_input"
  local matches=()
  for root in "${roots[@]}"; do
    [[ -n "$root" ]] || continue
    [[ "$root" == *"-${version}/"* || "$root" == *"-${version}" ]] || continue
    [[ -d "$root/$suffix" ]] || continue
    matches+=("$root/$suffix")
  done
  join_colon_paths "${matches[@]}"
}

configure_isolated_qt_paths() {
  [[ "${JELLYFIN_SMOKE_INHERIT_QT_PATHS:-0}" != "1" ]] || return 0
  command -v qtpaths6 >/dev/null 2>&1 || return 0

  local qt_version qt_plugins qt_qml
  qt_version="$(qtpaths6 -query QT_VERSION 2>/dev/null || true)"
  qt_plugins="$(qtpaths6 -query QT_INSTALL_PLUGINS 2>/dev/null || true)"
  qt_qml="$(qtpaths6 -query QT_INSTALL_QML 2>/dev/null || true)"
  [[ -n "$qt_version" ]] || return 0

  local qml_paths=()
  local plugin_paths=()
  local filtered_qml filtered_plugins qmake_qml qmake_plugins
  filtered_qml="$(filter_qt_version_paths "${NIXPKGS_QT6_QML_IMPORT_PATH:-}" "$qt_version")"
  filtered_plugins="$(filter_qt_version_paths "${QT_PLUGIN_PATH:-}" "$qt_version")"
  qmake_qml="$(qt_version_subdirs_from_roots "${QMAKEPATH:-}" "$qt_version" "lib/qt-6/qml")"
  qmake_plugins="$(qt_version_subdirs_from_roots "${QMAKEPATH:-}" "$qt_version" "lib/qt-6/plugins")"

  [[ -n "$qt_qml" && -d "$qt_qml" ]] && qml_paths+=("$qt_qml")
  [[ -n "$qmake_qml" ]] && qml_paths+=("$qmake_qml")
  [[ -n "$filtered_qml" ]] && qml_paths+=("$filtered_qml")

  [[ -n "$qt_plugins" && -d "$qt_plugins" ]] && plugin_paths+=("$qt_plugins")
  [[ -n "$qmake_plugins" ]] && plugin_paths+=("$qmake_plugins")
  [[ -n "$filtered_plugins" ]] && plugin_paths+=("$filtered_plugins")

  export QML2_IMPORT_PATH="$(join_colon_paths "${qml_paths[@]}")"
  export QML_IMPORT_PATH="$QML2_IMPORT_PATH"
  export NIXPKGS_QT6_QML_IMPORT_PATH="$QML2_IMPORT_PATH"
  export QT_PLUGIN_PATH="$(join_colon_paths "${plugin_paths[@]}")"
}

export HOME="$work/home"
export XDG_CACHE_HOME="$work/cache"
export XDG_CONFIG_HOME="$work/config"
export XDG_DATA_HOME="$work/data"
export XDG_RUNTIME_DIR="$work/runtime"
export JELLYFIN_DIAGNOSTICS_DIR="$work/diagnostics"
export QT_QPA_PLATFORM="${JELLYFIN_SMOKE_QPA_PLATFORM:-offscreen}"
export QT_QUICK_BACKEND="${QT_QUICK_BACKEND:-software}"
export QSG_RHI_BACKEND="${QSG_RHI_BACKEND:-opengl}"
if [[ "$bundled_app" == "1" ]]; then
  unset QT_PLUGIN_PATH QML_IMPORT_PATH QML2_IMPORT_PATH NIXPKGS_QT6_QML_IMPORT_PATH QMAKEPATH
  unset DYLD_LIBRARY_PATH DYLD_FRAMEWORK_PATH DYLD_FALLBACK_LIBRARY_PATH DYLD_FALLBACK_FRAMEWORK_PATH
else
  configure_isolated_qt_paths
fi
export LC_NUMERIC=C

"$candidate" --smoke-and-exit
