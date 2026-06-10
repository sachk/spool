#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
# shellcheck source=tools/lib/build-common.sh
source "$ROOT/tools/lib/build-common.sh"

scanner="$(resolve_qmlimportscanner "${QML_BUILD_NINJA:-}")"
if [[ -z "$scanner" || ! -x "$scanner" ]]; then
  echo "error: qmlimportscanner not found" >&2
  exit 1
fi

import_paths=()
add_import_path() {
  local path="$1"
  [[ -d "$path" ]] || return 0
  case ":${import_paths[*]}:" in
    *":$path:"*) ;;
    *) import_paths+=("$path") ;;
  esac
}

scanner_prefix="$(cd "$(dirname "$scanner")/.." && pwd)"
add_import_path "$scanner_prefix/lib/qt-6/qml"
add_import_path "${JELLYFIN_QT_VIRTUAL_KEYBOARD_QML_ROOT:-}"

roots=()
IFS=':;' read -r -a roots <<<"${CMAKE_PREFIX_PATH:-}"
for prefix in "${roots[@]}"; do
  add_import_path "$prefix/lib/qt-6/qml"
done

for variable in NIXPKGS_QT6_QML_IMPORT_PATH QML_IMPORT_PATH QML2_IMPORT_PATH; do
  paths=()
  IFS=: read -r -a paths <<<"${!variable:-}"
  for path in "${paths[@]}"; do
    add_import_path "$path"
  done
done

args=(-rootPath "$ROOT/qml")
for path in "${import_paths[@]}"; do
  args+=(-importPath "$path")
done

output="$(mktemp)"
trap 'rm -f "$output"' EXIT
"$scanner" "${args[@]}" >"$output"

python3 - "$output" <<'PY'
import json
import sys

with open(sys.argv[1], encoding="utf-8") as handle:
    imports = json.load(handle)

missing = sorted({
    entry["name"]
    for entry in imports
    if entry.get("type") == "module"
    and entry.get("name", "").startswith("Qt")
    and not entry.get("path")
})
if missing:
    print("error: unresolved QML modules:", file=sys.stderr)
    for name in missing:
        print(f"  {name}", file=sys.stderr)
    raise SystemExit(1)

plugins = sorted({
    entry["linkTarget"]
    for entry in imports
    if entry.get("linkTarget")
})
print(f"QML import scan resolved {len(plugins)} plugin targets")
for plugin in plugins:
    print(f"  {plugin}")
PY
