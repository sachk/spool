#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MODE="${1:-worktree}"

case "$MODE" in
  worktree|--staged) ;;
  *)
    echo "usage: $0 [--staged]" >&2
    exit 2
    ;;
esac

read_file() {
  if [[ "$MODE" == "--staged" ]]; then
    git -C "$ROOT" show ":$1"
  else
    cat "$ROOT/$1"
  fi
}

if ! git -C "$ROOT" ls-files --stage -- mpv | grep -q '^160000 '; then
  echo "error: mpv is not recorded as a git submodule" >&2
  exit 1
fi

if ! read_file .gitmodules | grep -A3 -F '[submodule "mpv"]' \
    | grep -q 'branch = network-threaded-mpv'; then
  echo "error: mpv must track the network-threaded-mpv branch" >&2
  exit 1
fi

if ! read_file flake.nix | grep -A3 '^[[:space:]]*mpv-src = {' \
    | grep -Fq 'url = "path:./mpv";'; then
  echo "error: flake.nix must source mpv from path:./mpv" >&2
  exit 1
fi

locked_path="$(read_file flake.lock | python3 -c '
import json
import sys

lock = json.load(sys.stdin)
node = lock["nodes"]["root"]["inputs"]["mpv-src"]
print(lock["nodes"][node]["locked"].get("path", ""))
')"

if [[ "$locked_path" != "./mpv" ]]; then
  echo "error: flake.lock must resolve mpv-src to ./mpv" >&2
  exit 1
fi

echo "mpv submodule and flake input are unified"
