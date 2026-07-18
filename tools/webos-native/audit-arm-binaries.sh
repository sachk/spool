#!/usr/bin/env bash
set -euo pipefail

if (( $# < 4 )); then
  echo "usage: $0 <readelf> <objdump> <--thumb|--thumb-archive|--stripped> <path>..." >&2
  exit 2
fi

READELF_BIN="$1"
OBJDUMP_BIN="$2"
MODE="$3"
shift 3

[[ "$MODE" == "--thumb" || "$MODE" == "--thumb-archive" || "$MODE" == "--stripped" ]] || {
  echo "error: unknown ARM audit mode: $MODE" >&2
  exit 2
}

for elf in "$@"; do
  [[ -f "$elf" ]] || {
    echo "error: ARM audit input is missing: $elf" >&2
    exit 1
  }

  if [[ "$MODE" == "--thumb-archive" ]]; then
    attributes="$("$READELF_BIN" -A "$elf")"
    [[ "$attributes" == *"Tag_THUMB_ISA_use: Thumb-2"* ]] || {
      echo "error: archive does not advertise Thumb-2 code: $elf" >&2
      exit 1
    }
    thumb_address="$("$READELF_BIN" -Ws "$elf" \
      | awk '$4 == "FUNC" && $2 ~ /[13579bBdDfF]$/ && !found { print $2; found = 1 }')"
    [[ -n "$thumb_address" ]] || {
      echo "error: archive has no Thumb function marker: $elf" >&2
      exit 1
    }
    if ! "$OBJDUMP_BIN" -d -M force-thumb "$elf" \
      | grep -E '^[[:space:]]*[0-9a-f]+:[[:space:]]+[0-9a-f]{4}([[:space:]]|$)' >/dev/null; then
      echo "error: no Thumb instruction sample found in archive: $elf" >&2
      exit 1
    fi
    continue
  fi

  header="$("$READELF_BIN" -h "$elf")"
  [[ "$header" == *"Class:"*"ELF32"* && "$header" == *"Machine:"*"ARM"* ]] || {
    echo "error: expected a 32-bit ARM ELF: $elf" >&2
    exit 1
  }

  if [[ "$MODE" == "--stripped" ]]; then
    sections="$("$READELF_BIN" -S "$elf")"
    [[ "$sections" != *".debug_"* && "$sections" != *".symtab"* ]] || {
      echo "error: staged release ELF still contains debug or static symbol sections: $elf" >&2
      exit 1
    }
    continue
  fi

  attributes="$("$READELF_BIN" -A "$elf")"
  [[ "$attributes" == *"Tag_THUMB_ISA_use: Thumb-2"* ]] || {
    echo "error: ELF does not advertise Thumb-2 code: $elf" >&2
    exit 1
  }

  thumb_address="$("$READELF_BIN" -Ws "$elf" \
    | awk '$4 == "FUNC" && $2 ~ /[13579bBdDfF]$/ && !found { print $2; found = 1 }')"
  if [[ -z "$thumb_address" ]]; then
    entry_address="$(awk '/Entry point address:/ { print $4 }' <<<"$header")"
    if [[ "$entry_address" =~ [13579bBdDfF]$ ]]; then
      thumb_address="${entry_address#0x}"
    fi
  fi
  [[ -n "$thumb_address" ]] || {
    echo "error: ELF has no Thumb function marker to sample: $elf" >&2
    exit 1
  }

  thumb_start=$((16#$thumb_address & ~1))
  thumb_stop=$((thumb_start + 8))
  disassembly="$("$OBJDUMP_BIN" -d -M force-thumb \
    --start-address="$thumb_start" --stop-address="$thumb_stop" "$elf")"
  [[ "$disassembly" =~ [[:space:]][0-9a-f]{4}[[:space:]] ]] || {
    echo "error: no Thumb instruction sample found at 0x$thumb_address in $elf" >&2
    exit 1
  }
done

echo "ARM $MODE audit passed: ${#@} ELFs"
