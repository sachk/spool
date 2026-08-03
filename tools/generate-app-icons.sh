#!/usr/bin/env bash
set -euo pipefail

APP_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ICON_ROOT="$APP_ROOT/app/icons"
PNG_ROOT="$ICON_ROOT/png"
PNG_SIZES=(16 20 22 24 32 40 48 64 80 128 130 256 512 1024)
WINDOWS_ICON_SIZES=(16 20 24 32 40 48 64 128 256)

if ! command -v magick >/dev/null 2>&1 || ! command -v oxipng >/dev/null 2>&1; then
  printf -v quoted_script '%q' "$0"
  exec nix-shell -p imagemagick oxipng --run "exec bash $quoted_script"
fi

tmp_dir="$(mktemp -d)"
trap 'rm -rf "$tmp_dir"' EXIT
mkdir -p "$PNG_ROOT/spool" "$PNG_ROOT/spool-film"

for variant in spool spool-film; do
  master="$tmp_dir/${variant}-8192.miff"
  magick -background none -density 16384 "$ICON_ROOT/${variant}.svg" \
    -resize 8192x8192\! "$master"
  for size in "${PNG_SIZES[@]}"; do
    magick "$master" -colorspace RGB -filter Lanczos -define filter:blur=0.989102836 \
      -resize "${size}x${size}!" -colorspace sRGB -depth 8 -strip \
      -define png:color-type=6 "$PNG_ROOT/$variant/$size.png"
  done
done

oxipng --opt max --strip safe --alpha --zopfli \
  "$PNG_ROOT/spool/"*.png "$PNG_ROOT/spool-film/"*.png

windows_frames=()
for size in "${WINDOWS_ICON_SIZES[@]}"; do
  windows_frames+=("$PNG_ROOT/spool/$size.png")
done
magick "${windows_frames[@]}" "$ICON_ROOT/spool.ico"
