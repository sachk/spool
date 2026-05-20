#!/usr/bin/env bash
set -euo pipefail

TARGET="${1:-root@192.168.0.200}"
APP_ID="${APP_ID:-com.sachk.tern}"
OUT="${OUT:-diagnostics-bundle-$(date -u +%Y%m%d-%H%M%S).tar.gz}"

ssh -tt "$TARGET" "rm -rf /tmp/${APP_ID}-diagnostics-bundle; mkdir -p /tmp/${APP_ID}-diagnostics-bundle; cp -a /tmp/${APP_ID}-diagnostics /tmp/${APP_ID}-diagnostics-bundle/tmp-diagnostics 2>/dev/null || true; cp -a /var/palm/data/${APP_ID}/diagnostics /tmp/${APP_ID}-diagnostics-bundle/appdata-diagnostics 2>/dev/null || true; cp -a /tmp/${APP_ID}.log /tmp/com.codex.jellyfinnative-mpv.log /tmp/${APP_ID}-diagnostics-bundle/ 2>/dev/null || true; ps > /tmp/${APP_ID}-diagnostics-bundle/ps.txt; tar -czf /tmp/${APP_ID}-diagnostics-bundle.tar.gz -C /tmp ${APP_ID}-diagnostics-bundle"
scp "$TARGET:/tmp/${APP_ID}-diagnostics-bundle.tar.gz" "$OUT"
printf 'Wrote %s\n' "$OUT"
