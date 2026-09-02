#!/usr/bin/env bash
# Run the render benchmark on a real webOS television and bring the numbers back.
#
# This is deliberately not part of ctest. It needs a rooted television with the
# app installed, a signed-in Jellyfin server, and a screen that is on -- none of
# which CI has, and all of which make it a manual measurement rather than a
# gate. tools/compare-render-benchmark.py is the gate that does run in CI.
#
# The app is started through the application manager rather than over ssh. A
# binary launched by hand runs as root, misses the environment the television
# hands its apps, and never comes to the foreground; it also leaves root-owned
# files in the app's own data directories, after which the app -- which is not
# root -- can no longer write its settings or database. So the benchmark's
# environment is handed over in a launch wrapper instead, and the wrapper and
# the appinfo that points at it are always put back.
#
# Usage:
#   tools/webos/bench-device.sh                                  # warm routes
#   tools/webos/bench-device.sh --cold
#   tools/webos/bench-device.sh --mode library --library Movies
#   tools/webos/bench-device.sh --mode library --library Movies --list --cold
set -euo pipefail

HOST="${SPOOL_TV_HOST:-root@192.168.0.200}"
APP_ID="com.sachk.spool"
APP_DIR="/media/developer/apps/usr/palm/applications/$APP_ID"
MODE="routes"
ITERATIONS="6"
LIBRARY="Movies"
COLD=""
LIST_MODE=""
SCROLL_STEPS=""
SCRIPT_ROUTES=""
OUT_DIR="${SPOOL_BENCH_DIR:-dist/tv-benchmark}"
WAIT_SECONDS="240"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --host) HOST="$2"; shift 2 ;;
    --mode) MODE="$2"; shift 2 ;;
    --iterations) ITERATIONS="$2"; shift 2 ;;
    --library) LIBRARY="$2"; shift 2 ;;
    --cold) COLD="1"; shift ;;
    --list) LIST_MODE="1"; shift ;;
    --scroll-steps) SCROLL_STEPS="$2"; shift 2 ;;
    --script) SCRIPT_ROUTES="$2"; MODE="$2"; shift 2 ;;
    --out) OUT_DIR="$2"; shift 2 ;;
    --wait) WAIT_SECONDS="$2"; shift 2 ;;
    -h|--help) sed -n '2,20p' "$0"; exit 0 ;;
    *) echo "unknown argument: $1" >&2; exit 2 ;;
  esac
done

label="${SCRIPT_ROUTES:+walk}"
[[ -z "$label" ]] && label="$MODE"
[[ -n "$COLD" ]] && label="$label-cold" || label="$label-warm"
[[ -n "$LIST_MODE" ]] && label="$label-list"
remote_out="/tmp/spool-bench-$label.json"
mkdir -p "$OUT_DIR"
local_out="$OUT_DIR/$label.json"

tv() { ssh -o StrictHostKeyChecking=no "$HOST" "$@"; }
luna() {
  local uri="$1"
  local payload="${2:-{\}}"
  ssh -F /dev/null -tt -o StrictHostKeyChecking=no "$HOST" \
    "${LUNA_BIN:-luna-send} -n 1 '$uri' '$payload'" 2>&1 | tr -d '\r' | sed 's/^/    luna: /'
}

restore() {
  echo "==> restoring $APP_ID"
  luna "luna://com.webos.applicationManager/closeByAppId" "{\"id\":\"$APP_ID\"}" || true
  tv "cd '$APP_DIR/bin' \
      && if [ -f jellyfin-native.real ]; then mv -f jellyfin-native.real jellyfin-native; fi \
      && cd '$APP_DIR' \
      && true \
      && chown -R 5486:5000 .config .cache .local 2>/dev/null || true" >/dev/null 2>&1 || true
}
trap restore EXIT

env_lines=(
  "export SPOOL_BENCH='$MODE'"
  "export SPOOL_BENCH_ITERATIONS='$ITERATIONS'"
  "export SPOOL_BENCH_OUT='$remote_out'"
)
[[ -n "$COLD" ]] && env_lines+=("export SPOOL_BENCH_COLD=1")
# The library is opened whenever one is named, not only in library mode, so a
# custom walk can measure switches with a real library on screen.
env_lines+=("export SPOOL_BENCH_LIBRARY='$LIBRARY'")
[[ -n "$LIST_MODE" ]] && env_lines+=("export SPOOL_BENCH_LIST_MODE=1")
[[ -n "$SCROLL_STEPS" ]] && env_lines+=("export SPOOL_BENCH_SCROLL_STEPS='$SCROLL_STEPS'")

echo "==> preparing $label on $HOST"
luna "luna://com.webos.applicationManager/closeByAppId" "{\"id\":\"$APP_ID\"}" || true
tv "cd '$APP_DIR/bin' \
    && rm -f '$remote_out' \
    && { [ -f jellyfin-native.real ] || mv jellyfin-native jellyfin-native.real; } \
    && printf '%s\n' '#!/bin/sh' $(printf "'%s' " "${env_lines[@]}") 'exec \"\$(dirname \"\$0\")/jellyfin-native.real\" \"\$@\"' > jellyfin-native \
    && chmod 755 jellyfin-native"

echo "==> launching through the application manager"
luna "luna://com.webos.applicationManager/launch" "{\"id\":\"$APP_ID\"}"

echo "==> waiting up to ${WAIT_SECONDS}s for $remote_out"
deadline=$(( $(date +%s) + WAIT_SECONDS ))
until tv "[ -s '$remote_out' ]" 2>/dev/null; do
  if [[ $(date +%s) -ge $deadline ]]; then
    echo "error: no report at $remote_out -- the run did not finish" >&2
    tv "tail -25 '$APP_DIR/.cache/logs/$APP_ID.log' 2>/dev/null" | sed 's/^/    /' || true
    exit 1
  fi
  sleep 5
done

scp -o StrictHostKeyChecking=no "$HOST:$remote_out" "$local_out" >/dev/null
echo "==> $local_out"
python3 tools/webos/summarise-device-benchmark.py "$local_out"
