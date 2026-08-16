#!/usr/bin/env bash
set -euo pipefail

target="${1:?usage: sign-macos.sh APP_BUNDLE_OR_DMG}"
: "${MACOS_CERTIFICATE_P12:?MACOS_CERTIFICATE_P12 is required}"
: "${MACOS_CERTIFICATE_PASSWORD:?MACOS_CERTIFICATE_PASSWORD is required}"
: "${MACOS_SIGNING_IDENTITY:?MACOS_SIGNING_IDENTITY is required}"
[[ -d "$target" || -f "$target" ]] || { printf 'signing target not found: %s\n' "$target" >&2; exit 1; }

work="$(mktemp -d)"
keychain="$work/release-signing.keychain-db"
keychain_password="$(openssl rand -hex 24)"
cleanup() {
  security delete-keychain "$keychain" >/dev/null 2>&1 || true
  rm -rf "$work"
}
trap cleanup EXIT

sort_nul() {
  python3 -c 'import sys; values=[v for v in sys.stdin.buffer.read().split(b"\0") if v]; sys.stdout.buffer.write(b"\0".join(sorted(values)) + (b"\0" if values else b""))'
}

sort_nul_deepest_first() {
  python3 -c 'import sys; values=[v for v in sys.stdin.buffer.read().split(b"\0") if v]; values.sort(key=lambda v: (-v.count(b"/"), v)); sys.stdout.buffer.write(b"\0".join(values) + (b"\0" if values else b""))'
}

codesign_with_timestamp() {
  local attempt output status
  for attempt in 1 2 3 4 5; do
    if output="$(codesign "$@" 2>&1)"; then
      [[ -z "$output" ]] || printf '%s\n' "$output" >&2
      return 0
    else
      status=$?
    fi
    [[ -z "$output" ]] || printf '%s\n' "$output" >&2
    if [[ "$output" != *"The timestamp service is not available."* || "$attempt" == "5" ]]; then
      return "$status"
    fi
    printf 'Apple timestamp service unavailable; retrying codesign in %d seconds (attempt %d/5)\n' \
      "$((attempt * 5))" "$attempt" >&2
    sleep "$((attempt * 5))"
  done
}

printf '%s' "$MACOS_CERTIFICATE_P12" | base64 --decode >"$work/certificate.p12"
chmod 600 "$work/certificate.p12"
security create-keychain -p "$keychain_password" "$keychain"
security list-keychains -d user -s "$keychain"
security set-keychain-settings -lut 21600 "$keychain"
security unlock-keychain -p "$keychain_password" "$keychain"
security import "$work/certificate.p12" -k "$keychain" -P "$MACOS_CERTIFICATE_PASSWORD" -T /usr/bin/codesign
security set-key-partition-list -S apple-tool:,apple:,codesign: -s -k "$keychain_password" "$keychain"
if [[ -f "$target" ]]; then
  [[ "$target" == *.dmg ]] || {
    printf 'unsupported signing target: %s\n' "$target" >&2
    exit 1
  }
  codesign_with_timestamp --force --timestamp --sign "$MACOS_SIGNING_IDENTITY" --keychain "$keychain" "$target"
  codesign --verify --strict --verbose=2 "$target"
  exit 0
fi

app="$target"

while IFS= read -r -d '' file; do
  if [[ "$file" == *.dylib || "$file" == *.so ]] || file "$file" | grep -Eq 'Mach-O'; then
    codesign_with_timestamp --force --options runtime --timestamp --sign "$MACOS_SIGNING_IDENTITY" --keychain "$keychain" "$file"
  fi
done < <(find "$app" -type f \( -perm -111 -o -name '*.dylib' -o -name '*.so' \) -print0 | sort_nul)
while IFS= read -r -d '' bundle; do
  [[ "$bundle" == "$app" ]] && continue
  codesign_with_timestamp --force --options runtime --timestamp --sign "$MACOS_SIGNING_IDENTITY" --keychain "$keychain" "$bundle"
done < <(find "$app" -depth -type d \( -name '*.framework' -o -name '*.app' -o -name '*.xpc' \) -print0 \
  | sort_nul_deepest_first)
gnu_iconv="$app/Contents/Frameworks/libiconv-gnu.2.dylib"
[[ -f "$gnu_iconv" && ! -L "$gnu_iconv" ]] || {
  printf 'GNU libiconv is not a regular bundled file: %s\n' "$gnu_iconv" >&2
  exit 1
}
codesign --verify --strict --verbose=2 "$gnu_iconv"
codesign_with_timestamp --force --deep --options runtime --timestamp --sign "$MACOS_SIGNING_IDENTITY" --keychain "$keychain" "$app"
codesign --verify --strict --deep --verbose=2 "$app"
