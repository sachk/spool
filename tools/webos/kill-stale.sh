#!/usr/bin/env bash
set -euo pipefail

TARGET="${1:-root@192.168.0.200}"
APP_ID="${APP_ID:-com.sachk.tern}"

ssh -tt "$TARGET" "pids=\$(ps | grep '$APP_ID\|jellyfin-native' | grep -v grep | awk '{print \$1}' || true); if [ -z \"\$pids\" ]; then echo 'No matching processes'; exit 0; fi; echo \"Killing stale processes: \$pids\"; kill \$pids; sleep 2; for p in \$pids; do kill -0 \$p 2>/dev/null && kill -9 \$p || true; done"
