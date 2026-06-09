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
  local path="$1"
  if [[ "$MODE" == "--staged" ]]; then
    git -C "$ROOT" show ":$path"
  else
    cat "$ROOT/$path"
  fi
}

gitlink_revision() {
  local path="$1"
  git -C "$ROOT" ls-files --stage -- "$path" | awk '$1 == "160000" { print $2; exit }'
}

locked_revision() {
  local input="$1"
  read_file flake.lock | python3 -c '
import json
import sys

input_name = sys.argv[1]
lock = json.load(sys.stdin)
node_name = lock["nodes"]["root"]["inputs"].get(input_name, "")
print(lock["nodes"].get(node_name, {}).get("locked", {}).get("rev", ""))
' "$input"
}

flake_revision() {
  local input="$1"
  read_file flake.nix | sed -n \
    "/^[[:space:]]*$input = {/,/^[[:space:]]*};/s|.*github:sachk/mpv/\\([0-9a-f]\\{40\\}\\).*|\\1|p" \
    | head -n 1
}

check_source() {
  local path="$1"
  local branch="$2"
  local input="$3"
  local gitlink locked declared

  gitlink="$(gitlink_revision "$path")"
  locked="$(locked_revision "$input")"
  declared="$(flake_revision "$input")"

  [[ -n "$gitlink" ]] || {
    echo "error: $path is not recorded as a git submodule" >&2
    return 1
  }
  [[ -n "$locked" ]] || {
    echo "error: flake.lock does not contain $input" >&2
    return 1
  }
  [[ -n "$declared" ]] || {
    echo "error: flake.nix does not pin $input to an exact mpv revision" >&2
    return 1
  }

  if [[ "$gitlink" != "$declared" || "$gitlink" != "$locked" ]]; then
    printf 'error: %s revision mismatch\n' "$path" >&2
    printf '  submodule: %s\n  flake.nix: %s\n  flake.lock: %s\n' \
      "$gitlink" "$declared" "$locked" >&2
    return 1
  fi

  if ! read_file .gitmodules | grep -A3 -F "[submodule \"$path\"]" \
      | grep -q "branch = $branch"; then
    echo "error: $path must track the $branch branch in .gitmodules" >&2
    return 1
  fi
}

if [[ "$MODE" == "--staged" ]]; then
  for path in mpv mpv_webos; do
    if ! git -C "$ROOT" diff --cached --quiet HEAD -- "$path"; then
      for required in flake.nix flake.lock; do
        if git -C "$ROOT" diff --cached --quiet HEAD -- "$required"; then
          echo "error: staged $path pointer change requires staged $required" >&2
          exit 1
        fi
      done
    fi
  done
fi

check_source mpv libmpv-gpu-next mpv-src
check_source mpv_webos webos mpv-webos-src

echo "mpv submodules and flake inputs are in sync"
