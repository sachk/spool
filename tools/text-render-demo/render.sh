#!/usr/bin/env bash
# Draw the year/director line in every text rendering, hinting and
# antialiasing mode Qt offers, at the size, weight and colour webOS actually
# uses for it, over black and over a frame.
#
#   nix develop .#native -c bash tools/text-render-demo/render.sh \
#     --frame path/to/video-or-still
#
# The television renders the interface at 1080p and the panel nearest-neighbour
# scales that to 4K, so two sheets come out: the 1080p pixels the scene
# composes, and those pixels doubled unfiltered, which is what the panel puts
# in front of you.
#
# It runs against the Qt the app is built with, on llvmpipe under Xvfb,
# because the scene graph has to be the real OpenGL one: the software backend
# draws every renderType identically and would answer nothing.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
FRAME=""
OUT_DIR="$ROOT/build/text-render-demo"
# dp(22) at the television's 130% default: Metrics.chromeScale is 0.78 at
# 1080p, so the player overlay's scale is 1.014 and the line lands on 22px.
SAMPLE="1999 · Directed by David Fincher"
FRAME_SEEK="${SPOOL_DEMO_FRAME_SEEK:-00:22:30}"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --frame) FRAME="$2"; shift 2 ;;
    --out) OUT_DIR="$2"; shift 2 ;;
    --sample) SAMPLE="$2"; shift 2 ;;
    *) echo "error: unknown argument: $1" >&2; exit 2 ;;
  esac
done

[[ -n "$FRAME" ]] || { echo "error: --frame is required (a still, or any video to take one from)" >&2; exit 2; }
[[ -e "$FRAME" ]] || { echo "error: no such frame source: $FRAME" >&2; exit 1; }

# The app's Qt, not whatever is first on PATH: a mismatched qtdeclarative
# cannot even load against it, and a different Qt would not be answering the
# question that was asked.
qt_prefix_for() {
  python3 -c '
import os
import sys

for entry in os.environ.get("CMAKE_PREFIX_PATH", "").split(":"):
    if sys.argv[1] in os.path.basename(entry):
        print(entry)
        break
' "$1"
}

QTBASE="$(qt_prefix_for qtbase)"
QTDECLARATIVE="$(qt_prefix_for qtdeclarative)"
[[ -n "$QTDECLARATIVE" && -x "$QTDECLARATIVE/bin/qml" ]] || {
  echo "error: run this through 'nix develop .#native -c ...' so the app's Qt is on CMAKE_PREFIX_PATH" >&2
  exit 1
}

mkdir -p "$OUT_DIR"
backdrop="$OUT_DIR/backdrop.png"
case "${FRAME,,}" in
  *.png | *.jpg | *.jpeg | *.webp) cp -f "$FRAME" "$backdrop" ;;
  *) ffmpeg -hide_banner -loglevel error -ss "$FRAME_SEEK" -i "$FRAME" -frames:v 1 -y "$backdrop" ;;
esac

sheet="$OUT_DIR/modes-1080p.png"
rm -f "$sheet"

Xvfb :99 -screen 0 3800x1000x24 +extension GLX &
xvfb=$!
trap 'kill "$xvfb" 2>/dev/null || true' EXIT
sleep 2

env DISPLAY=:99 LANG=C.UTF-8 \
  QT_PLUGIN_PATH="$QTBASE/lib/qt-6/plugins:$QTDECLARATIVE/lib/qt-6/plugins" \
  QML2_IMPORT_PATH="$QTDECLARATIVE/lib/qt-6/qml" \
  QT_QPA_PLATFORM=xcb QSG_RHI_BACKEND=opengl LIBGL_ALWAYS_SOFTWARE=1 \
  "$QTDECLARATIVE/bin/qml" "$HERE/TextModes.qml" -- \
  "--font=$ROOT/qml/fonts/PTRootUI-Variable.ttf" \
  "--frame=$backdrop" \
  "--out=$sheet" \
  "--sample=$SAMPLE" 2>&1 | grep -viE "xkbcomp|warning:|keysym|^>|not fatal" || true

[[ -f "$sheet" ]] || { echo "error: the sheet was not rendered" >&2; exit 1; }

# What the panel shows: the same pixels, doubled, unfiltered.
magick "$sheet" -filter point -resize 200% "$OUT_DIR/modes-4k.png"

printf 'text rendering sheets:\n'
printf '  %s\n' "$sheet" "$OUT_DIR/modes-4k.png"
