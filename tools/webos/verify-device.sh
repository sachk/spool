#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"

host="${TV_HOST:-root@192.168.0.200}"
device="${ARES_DEVICE:-}"
app_id="${APP_ID:-com.sachk.tern}"
ipk=""
outdir=""
install=1
screenshot=1
launch_wait="${LAUNCH_WAIT:-8}"
luna_bin="${LUNA_BIN:-luna-send}"

usage() {
  cat <<EOF
Usage: tools/webos/verify-device.sh [options] [path/to/app.ipk]

Installs and launches the native webOS package, verifies app registration,
captures current app logs, and captures a screenshot using supported webOS
services. It does not remove app data, patch TV files, or restart services.

Options:
  --host HOST          SSH target for LS2/log access (default: $host)
  --device NAME        ares device name; omitted means ares default
  --app-id ID          app id (default: $app_id)
  --ipk PATH           IPK to install; defaults to newest matching build IPK
  --out DIR            output directory (default: build/webos/verify/<timestamp>)
  --no-install         skip ares-install and only verify/launch/capture
  --skip-screenshot   skip screenshot capture
  --launch-wait SEC    seconds to wait after launch before capture (default: $launch_wait)
  -h, --help           show this help
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --host) host="$2"; shift 2 ;;
    --device) device="$2"; shift 2 ;;
    --app-id) app_id="$2"; shift 2 ;;
    --ipk) ipk="$2"; shift 2 ;;
    --out) outdir="$2"; shift 2 ;;
    --no-install) install=0; shift ;;
    --skip-screenshot) screenshot=0; shift ;;
    --launch-wait) launch_wait="$2"; shift 2 ;;
    -h|--help) usage; exit 0 ;;
    --*) echo "unknown option: $1" >&2; usage >&2; exit 2 ;;
    *)
      if [[ -n "$ipk" ]]; then
        echo "unexpected extra argument: $1" >&2
        usage >&2
        exit 2
      fi
      ipk="$1"
      shift
      ;;
  esac
done

require_command() {
  command -v "$1" >/dev/null 2>&1 || {
    echo "missing required command: $1" >&2
    exit 1
  }
}

find_default_ipk() {
  local candidate
  candidate="$(
    find "$ROOT/build" "$ROOT/dist" -type f -name "${app_id}_*.ipk" -print 2>/dev/null \
      | sort \
      | tail -n 1
  )"
  [[ -n "$candidate" ]] && printf '%s\n' "$candidate"
}

run_luna() {
  local uri="$1"
  local payload="${2:-{}}"
  ssh -F /dev/null -tt "$host" "$luna_bin -n 1 '$uri' '$payload'"
}

ares_args=()
if [[ -n "$device" ]]; then
  ares_args+=(--device "$device")
fi

if [[ "$install" == "1" ]]; then
  [[ -n "$ipk" ]] || ipk="$(find_default_ipk || true)"
  if [[ -z "$ipk" || ! -f "$ipk" ]]; then
    echo "no IPK found; pass --ipk PATH or use --no-install" >&2
    exit 1
  fi
  require_command ares-install
fi

require_command ares-launch
require_command ssh
require_command scp

if [[ -z "$outdir" ]]; then
  outdir="$ROOT/build/webos/verify/$(date -u +%Y%m%d-%H%M%S)"
fi
mkdir -p "$outdir"

echo "Output: $outdir"

if [[ "$install" == "1" ]]; then
  echo "Installing $ipk"
  ares-install "${ares_args[@]}" "$ipk" | tee "$outdir/ares-install.log"
fi

echo "Checking app registration for $app_id"
if ! run_luna "luna://com.webos.applicationManager/dev/listApps" "{}" >"$outdir/listApps.json" 2>"$outdir/listApps.err"; then
  run_luna "luna://com.webos.applicationManager/listApps" "{}" >"$outdir/listApps.json"
fi
if ! grep -Fq "$app_id" "$outdir/listApps.json"; then
  echo "app id $app_id was not present in applicationManager listApps output" >&2
  exit 1
fi
run_luna "luna://com.webos.service.applicationManager/getAppInfo" "{\"id\":\"$app_id\"}" \
  >"$outdir/getAppInfo.json" 2>"$outdir/getAppInfo.err" || true

echo "Launching $app_id"
ares-launch "${ares_args[@]}" "$app_id" | tee "$outdir/ares-launch.log"
sleep "$launch_wait"

echo "Capturing process snapshot and logs"
ssh -F /dev/null -o BatchMode=yes "$host" "ps | grep '$app_id\|jellyfin-native' | grep -v grep || true" \
  >"$outdir/ps.txt"
for remote in \
  "/tmp/${app_id}/${app_id}.log" \
  "/tmp/${app_id}/com.codex.jellyfinnative-mpv.log" \
  "/media/cryptofs/apps/usr/palm/applications/${app_id}/.cache/logs/${app_id}.log" \
  "/media/cryptofs/apps/usr/palm/applications/${app_id}/.cache/logs/com.codex.jellyfinnative-mpv.log" \
  "/tmp/${app_id}.log" \
  "/tmp/com.codex.jellyfinnative-mpv.log" \
  "/tmp/${app_id}-diagnostics/current-instance.json" \
  "/var/palm/data/${app_id}/diagnostics/current-instance.json"; do
  local_name="${remote#/}"
  local_name="${local_name//\//_}"
  if ssh -F /dev/null -o BatchMode=yes "$host" "test -f '$remote'"; then
    ssh -F /dev/null -o BatchMode=yes "$host" "cat '$remote'" >"$outdir/$local_name"
  fi
done

if [[ "$screenshot" == "1" ]]; then
  remote_shot="/tmp/${app_id}-verify-$(date -u +%Y%m%d-%H%M%S).png"
  payload="{\"path\":\"$remote_shot\",\"method\":\"DISPLAY\",\"format\":\"PNG\"}"
  echo "Capturing screenshot"
  if ! run_luna "luna://com.webos.service.capture/executeOneShot" "$payload" >"$outdir/screenshot.json" 2>"$outdir/screenshot.err"; then
    run_luna "luna://com.webos.service.tv.capture/executeOneShot" "$payload" \
      >"$outdir/screenshot.json"
  fi
  scp -F /dev/null -q "$host:$remote_shot" "$outdir/screenshot.png"
fi

echo "Verification artifacts written to $outdir"
