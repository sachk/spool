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

read -r mode submodule_rev stage path < <(git -C "$ROOT" ls-files --stage -- mpv)
if [[ "$mode" != "160000" || -z "$submodule_rev" ]]; then
  echo "error: mpv is not recorded as a git submodule" >&2
  exit 1
fi

if ! read_file .gitmodules | grep -A3 -F '[submodule "mpv"]' \
    | grep -q 'branch = universal'; then
  echo "error: mpv must track the universal branch" >&2
  exit 1
fi

if ! read_file flake.nix | grep -A3 '^[[:space:]]*mpv-src = {' \
    | grep -Fq 'url = "git+file:./mpv";'; then
  echo "error: flake.nix must source mpv from git+file:./mpv" >&2
  exit 1
fi

IFS=$'\t' read -r locked_type locked_url locked_rev < <(read_file flake.lock | python3 -c '
import json
import sys

lock = json.load(sys.stdin)
node = lock["nodes"]["root"]["inputs"]["mpv-src"]
locked = lock["nodes"][node]["locked"]
print(locked.get("type", ""), locked.get("url", ""), locked.get("rev", ""), sep="\t")
')

if [[ "$locked_type" != "git" || "$locked_url" != "file:./mpv" ]]; then
  echo "error: flake.lock must resolve mpv-src to git+file:./mpv" >&2
  exit 1
fi

if [[ "$locked_rev" != "$submodule_rev" ]]; then
  echo "error: flake.lock mpv-src revision $locked_rev does not match mpv submodule $submodule_rev" >&2
  echo "       Run: nix flake lock --update-input mpv-src" >&2
  exit 1
fi

echo "mpv submodule and flake input are unified"
