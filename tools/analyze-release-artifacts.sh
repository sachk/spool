#!/usr/bin/env bash
set -euo pipefail

asset_dir="${1:?usage: analyze-release-artifacts.sh ASSET_DIR REPORT_DIR}"
report_dir="${2:?usage: analyze-release-artifacts.sh ASSET_DIR REPORT_DIR}"
: "${SYFT:=syft}"
: "${GRYPE:=grype}"
mkdir -p "$report_dir"
shopt -s nullglob
assets=("$asset_dir"/*.AppImage "$asset_dir"/*.dmg "$asset_dir"/*-Portable.exe "$asset_dir"/*-Setup.exe "$asset_dir"/*.ipk)
(( ${#assets[@]} > 0 )) || { echo 'no release packages found' >&2; exit 1; }

for asset in "${assets[@]}"; do
  base="$(basename "$asset")"
  work="$(mktemp -d)"
  root="$work/root"
  mkdir -p "$root"
  case "$asset" in
    *.AppImage)
      chmod +x "$asset"
      (cd "$work" && "$OLDPWD/$asset" --appimage-extract >/dev/null)
      mv "$work/squashfs-root" "$root/appimage"
      ;;
    *.dmg|*-Portable.exe|*-Setup.exe)
      7z x -y -o"$root" "$asset" >/dev/null
      ;;
    *.ipk)
      (cd "$work" && ar x "$OLDPWD/$asset")
      data_archive="$(find "$work" -maxdepth 1 -name 'data.tar.*' -print -quit)"
      [[ -n "$data_archive" ]] || { echo "missing IPK data archive: $base" >&2; exit 1; }
      tar -xf "$data_archive" -C "$root"
      ;;
  esac
  "$SYFT" "dir:$root" -o "cyclonedx-json=$report_dir/$base.cdx.json"
  "$GRYPE" "sbom:$report_dir/$base.cdx.json" -o json >"$report_dir/$base.grype.json"
  "$GRYPE" "sbom:$report_dir/$base.cdx.json" --fail-on high --only-fixed
  rm -rf "$work"
done
