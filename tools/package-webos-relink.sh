#!/usr/bin/env bash
set -euo pipefail

output_dir="${1:?Usage: package-webos-relink.sh OUTPUT_DIR}"
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
# shellcheck source=tools/lib/build-common.sh
source "$root/tools/lib/build-common.sh"
version="$(read_project_version "$root")"
archive="$output_dir/Jellyfin-Native-$version-webOS-relink.tar.gz"
list="$(mktemp)"
trap 'rm -f "$list"' EXIT
mkdir -p "$output_dir"

for tree in build/webos-app build/qt6-611-target-static-install build/webos-qcoro build/webos-thirdparty-build; do
  [[ -d "$root/$tree" ]] || continue
  find "$root/$tree" -type f \( -name '*.o' -o -name '*.a' -o -name '*.prl' -o -name '*.cmake' \
    -o -name '*.ninja' -o -name 'CMakeCache.txt' -o -name '*.h' -o -name '*.json' \) \
    -printf '%P\n' | sed "s#^#$tree/#" >>"$list"
done
for path in CMakeLists.txt VERSION LICENSE cmake tools app qml src; do
  [[ -e "$root/$path" ]] && printf '%s\n' "$path" >>"$list"
done
sort -u -o "$list" "$list"
[[ -s "$list" ]] || {
  echo 'error: no webOS relink material found' >&2
  exit 1
}
TZ=UTC tar --sort=name --mtime='@0' --owner=0 --group=0 --numeric-owner \
  -czf "$archive" -C "$root" --files-from="$list"
tar -tzf "$archive" >/dev/null
sha256sum "$archive" >"$archive.sha256"
printf '%s\n' "$archive"
