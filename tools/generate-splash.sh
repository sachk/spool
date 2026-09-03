#!/usr/bin/env bash
# Render the launch screen for one platform.
#
# The mark, the wordmark and the version are one block -- the "core" -- drawn
# on black. Everything that puts a launch screen on screen draws that same
# block centred, so the frame the operating system shows and the first frame
# Qt paints are the same picture and the handover is invisible.
#
#   tools/generate-splash.sh --platform desktop|webos|android-phone|android-tv \
#                            --out DIR [--version X.Y.Z]
#
# The version is part of the picture, so this runs per build rather than
# leaving a rendered PNG in the tree to go stale.
set -euo pipefail

APP_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MANIFEST="$APP_ROOT/tools/manifests/splash.json"
PLATFORM=""
OUT_DIR=""
VERSION=""

while [[ $# -gt 0 ]]; do
  case "$1" in
    --platform) PLATFORM="$2"; shift 2 ;;
    --out) OUT_DIR="$2"; shift 2 ;;
    --version) VERSION="$2"; shift 2 ;;
    *) echo "error: unknown argument: $1" >&2; exit 2 ;;
  esac
done

[[ -n "$PLATFORM" ]] || { echo "error: --platform is required" >&2; exit 2; }
[[ -n "$OUT_DIR" ]] || { echo "error: --out is required" >&2; exit 2; }
[[ -n "$VERSION" ]] || VERSION="$(tr -d '[:space:]' <"$APP_ROOT/VERSION")"

case "$PLATFORM" in
  desktop | webos | android-phone | android-tv) ;;
  *) echo "error: unsupported splash platform: $PLATFORM" >&2; exit 2 ;;
esac

# Same self-bootstrap as the icon generator: a machine without ImageMagick
# borrows one rather than making the caller install it first.
if ! command -v magick >/dev/null 2>&1; then
  if command -v nix-shell >/dev/null 2>&1; then
    quoted=()
    for arg in "$0" --platform "$PLATFORM" --out "$OUT_DIR" --version "$VERSION"; do
      printf -v one '%q' "$arg"
      quoted+=("$one")
    done
    exec nix-shell -p imagemagick --run "exec bash ${quoted[*]}"
  fi
  echo "error: ImageMagick (magick) is required to render the launch screen" >&2
  exit 1
fi

field() {
  python3 -c '
import json
import sys

with open(sys.argv[1], encoding="utf-8") as handle:
    value = json.load(handle)
for key in sys.argv[2].split("."):
    value = value[key]
print(value)
' "$MANIFEST" "$1"
}

SCALE="$(field renderScale)"
CORE_W="$(field core.width)"
CORE_H="$(field core.height)"
CORE_BG="$(field core.background)"
LOGO_SIZE="$(field logo.size)"
LOGO_OFFSET_Y="$(field logo.offsetY)"
WORDMARK_TEXT="$(field wordmark.text)"
WORDMARK_SIZE="$(field wordmark.pointSize)"
WORDMARK_WEIGHT="$(field wordmark.weight)"
WORDMARK_COLOR="$(field wordmark.color)"
WORDMARK_OFFSET_Y="$(field wordmark.offsetY)"
VERSION_SIZE="$(field version.pointSize)"
VERSION_WEIGHT="$(field version.weight)"
VERSION_COLOR="$(field version.color)"
VERSION_OFFSET_Y="$(field version.offsetY)"
VERSION_PREFIX="$(field version.prefix)"
REFERENCE_W="$(field reference.width)"
REFERENCE_H="$(field reference.height)"
ANDROID_SOURCE_W="$(field android.sourceWidth)"

scaled() { echo $(($1 * SCALE)); }

FONT="$APP_ROOT/qml/fonts/IBMPlexSans-Variable.ttf"
LOGO_SVG="$APP_ROOT/app/icons/spool.svg"

tmp_dir="$(mktemp -d)"
trap 'rm -rf "$tmp_dir"' EXIT
mkdir -p "$OUT_DIR"

# The mark is vector art; rasterise it once at the supersampled size and let
# the single downsample at the end do all the filtering.
logo="$tmp_dir/logo.png"
magick -background none -density 4096 "$LOGO_SVG" \
  -resize "$(scaled "$LOGO_SIZE")x$(scaled "$LOGO_SIZE")"\! "$logo"

render_core() {
  local width="$1" height="$2" destination="$3"
  magick -size "$(scaled "$CORE_W")x$(scaled "$CORE_H")" xc:"$CORE_BG" \
    "$logo" -gravity center -geometry "+0$(printf '%+d' "$(scaled "$LOGO_OFFSET_Y")")" -composite \
    -font "$FONT" -gravity center \
    -pointsize "$(scaled "$WORDMARK_SIZE")" -weight "$WORDMARK_WEIGHT" -fill "$WORDMARK_COLOR" \
    -annotate "+0$(printf '%+d' "$(scaled "$WORDMARK_OFFSET_Y")")" "$WORDMARK_TEXT" \
    -pointsize "$(scaled "$VERSION_SIZE")" -weight "$VERSION_WEIGHT" -fill "$VERSION_COLOR" \
    -annotate "+0$(printf '%+d' "$(scaled "$VERSION_OFFSET_Y")")" "$VERSION_PREFIX$VERSION" \
    -colorspace RGB -filter Lanczos -resize "${width}x${height}"\! \
    -colorspace sRGB -depth 8 -strip -define png:color-type=2 "$destination"
}

case "$PLATFORM" in
  desktop)
    # Qt scales this to the viewport, so ship it at the size the largest
    # screen asks for rather than letting it be enlarged.
    render_core "$ANDROID_SOURCE_W" "$((ANDROID_SOURCE_W * CORE_H / CORE_W))" "$OUT_DIR/splash-core.png"
    ;;
  webos)
    render_core "$ANDROID_SOURCE_W" "$((ANDROID_SOURCE_W * CORE_H / CORE_W))" "$OUT_DIR/splash-core.png"
    # webOS names a full-screen background in appinfo.json, and the panel is
    # always 16:9 1080p, so the whole frame can be rendered exactly.
    reference_core="$tmp_dir/core-reference.png"
    render_core "$CORE_W" "$CORE_H" "$reference_core"
    magick -size "${REFERENCE_W}x${REFERENCE_H}" xc:"$CORE_BG" \
      "$reference_core" -gravity center -composite \
      -depth 8 -strip -define png:color-type=2 "$OUT_DIR/splash.png"
    ;;
  android-phone | android-tv)
    if [[ "$PLATFORM" == android-tv ]]; then
      core_dp="$(field android.tvCoreWidthDp)"
    else
      core_dp="$(field android.phoneCoreWidthDp)"
    fi
    core_height_dp=$((core_dp * CORE_H / CORE_W))
    render_core "$ANDROID_SOURCE_W" "$((ANDROID_SOURCE_W * CORE_H / CORE_W))" "$OUT_DIR/splash-core.png"
    mkdir -p "$OUT_DIR/res/drawable-nodpi" "$OUT_DIR/res/drawable"
    cp -f "$OUT_DIR/splash-core.png" "$OUT_DIR/res/drawable-nodpi/spool_splash_core.png"
    # Black everywhere, the core centred at an explicit dp size. Qt reads the
    # same dp out of the manifest, so its first frame lands on the same
    # pixels this one did and the handover shows nothing.
    cat >"$OUT_DIR/res/drawable/spool_splash.xml" <<XML
<?xml version="1.0" encoding="utf-8"?>
<!-- Generated by tools/generate-splash.sh; do not edit. -->
<layer-list xmlns:android="http://schemas.android.com/apk/res/android">
    <item android:drawable="@android:color/black" />
    <item
        android:width="${core_dp}dp"
        android:height="${core_height_dp}dp"
        android:gravity="center">
        <bitmap android:src="@drawable/spool_splash_core" android:gravity="center" />
    </item>
</layer-list>
XML
    ;;
esac

printf 'launch screen for %s %s -> %s\n' "$PLATFORM" "$VERSION" "$OUT_DIR"
