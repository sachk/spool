#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

git -C "$ROOT" config core.hooksPath tools/git-hooks
echo "Configured core.hooksPath=tools/git-hooks (pre-commit + pre-push)"
