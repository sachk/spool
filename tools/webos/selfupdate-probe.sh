#!/usr/bin/env bash
# Ask a television whether an app may install an IPK by itself.
#
#   TV_HOST=root@tv.local tools/webos/selfupdate-probe.sh --build
#
# Builds a throwaway diagnostic IPK, serves a real IPK over HTTP from this
# machine, installs the diagnostic, launches it in the foreground, and prints
# what the bus said to each of the install routes it tried.
#
# Reading the result: LS2 refuses in two different ways and they mean opposite
# things. "Denied method call ... for security reasons" -- or a call that never
# dispatches at all -- is the role forbidding it, and no amount of retrying or
# rephrasing will change it. An error from the service itself means the call
# was allowed through and only the request was wrong, which is fixable. The
# control calls at the top exist to tell those apart from a probe that is
# simply broken: if the controls are denied too, the run says nothing.
#
# On a rooted set this measures a rooted set. The question is about a retail
# one, so treat a pass here as "worth trying on an unrooted set", never as the
# answer.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
app_id="com.sachk.spool"
host="${TV_HOST:-}"
do_build=0
serve_port="${SPOOL_PROBE_PORT:-8099}"
outdir="$ROOT/build/webos-selfupdate-probe"
settle="${SPOOL_PROBE_SETTLE:-30}"

usage() {
  cat <<EOF
usage: $(basename "$0") [--host HOST] [--build] [--ipk PATH] [--port N]

  --host HOST   ssh target for the television (or TV_HOST)
  --build       build the probe IPK first (implies a full webOS app build)
  --ipk PATH    IPK to serve as the update target; defaults to the newest built
  --port N      port to serve the target IPK on (default $serve_port)
EOF
}

ipk=""
while [[ $# -gt 0 ]]; do
  case "$1" in
    --host) host="$2"; shift 2 ;;
    --build) do_build=1; shift ;;
    --ipk) ipk="$2"; shift 2 ;;
    --port) serve_port="$2"; shift 2 ;;
    -h | --help) usage; exit 0 ;;
    *) echo "unexpected argument: $1" >&2; usage >&2; exit 2 ;;
  esac
done

[[ -n "$host" ]] || { echo "missing --host HOST (or TV_HOST)" >&2; exit 2; }

mkdir -p "$outdir"

# The television has to reach this machine, so bind the address it will use.
host_only="${host#*@}"
host_ip="$(ip route get "$(getent ahostsv4 "$host_only" | awk 'NR==1{print $1}')" 2>/dev/null \
  | awk '{for (i = 1; i < NF; i++) if ($i == "src") print $(i + 1)}' | head -n 1)"
[[ -n "$host_ip" ]] || { echo "could not work out this machine's address on the television's network" >&2; exit 1; }

if (( do_build )); then
  # The probe needs to know what to ask for before it is compiled, because a
  # webOS app gets no environment of its own to read at launch.
  SPOOL_WEBOS_SELFUPDATE_PROBE=1 \
    SPOOL_WEBOS_SELFUPDATE_PROBE_TARGET="http://$host_ip:$serve_port/spool-update.ipk" \
    "$ROOT/build-ipk.sh"
fi

find_newest_ipk() {
  find "$ROOT/build" "$ROOT/dist" -type f -name "${app_id}_*.ipk" -print 2>/dev/null | sort | tail -n 1
}

probe_ipk="$(find_newest_ipk)"
[[ -n "$probe_ipk" ]] || { echo "no IPK found; pass --build" >&2; exit 1; }
[[ -n "$ipk" ]] || ipk="$probe_ipk"

# Serve the target the probe will ask the television to fetch.
serve_root="$outdir/serve"
mkdir -p "$serve_root"
cp -f "$ipk" "$serve_root/spool-update.ipk"
python3 -m http.server "$serve_port" --directory "$serve_root" >"$outdir/server.log" 2>&1 &
server_pid=$!
trap 'kill "$server_pid" 2>/dev/null || true' EXIT
sleep 1

printf 'probe IPK:   %s\n' "$probe_ipk"
printf 'update IPK:  http://%s:%s/spool-update.ipk\n' "$host_ip" "$serve_port"

nix develop "$ROOT" -c bash "$ROOT/tools/webos/verify-device.sh" --host "$host" "$probe_ipk"

# The probe fires five seconds in and gives the calls twenty to answer.
printf 'waiting %ss for the probe to finish\n' "$settle"
sleep "$settle"

log_remote="/var/log/${app_id}.log"
ssh -F /dev/null "$host" "cat /tmp/${app_id}.log 2>/dev/null || cat '$log_remote' 2>/dev/null || true" \
  >"$outdir/app.log" || true
# The app's own log location varies by firmware; fall back to the journal.
if ! grep -q "selfupdate-probe" "$outdir/app.log" 2>/dev/null; then
  ssh -F /dev/null "$host" "journalctl -t ${app_id} --no-pager -n 2000 2>/dev/null \
    || logread 2>/dev/null || true" >>"$outdir/app.log" || true
fi

printf '\n--- probe transcript ---\n'
if grep -q "selfupdate-probe" "$outdir/app.log"; then
  grep "selfupdate-probe" "$outdir/app.log"
else
  echo "no probe output found; the full log is at $outdir/app.log" >&2
  exit 1
fi
