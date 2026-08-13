#!/usr/bin/env bash
set -euo pipefail

app="${1:?usage: sign-macos.sh APP_BUNDLE}"
: "${MACOS_CERTIFICATE_P12:?MACOS_CERTIFICATE_P12 is required}"
: "${MACOS_CERTIFICATE_PASSWORD:?MACOS_CERTIFICATE_PASSWORD is required}"
: "${MACOS_SIGNING_IDENTITY:?MACOS_SIGNING_IDENTITY is required}"
[[ -d "$app" ]] || { printf 'app bundle not found: %s\n' "$app" >&2; exit 1; }

work="$(mktemp -d)"
keychain="$work/release-signing.keychain-db"
keychain_password="$(openssl rand -hex 24)"
cleanup() {
  security delete-keychain "$keychain" >/dev/null 2>&1 || true
  rm -rf "$work"
}
trap cleanup EXIT

printf '%s' "$MACOS_CERTIFICATE_P12" | base64 --decode >"$work/certificate.p12"
chmod 600 "$work/certificate.p12"
security create-keychain -p "$keychain_password" "$keychain"
security list-keychains -d user -s "$keychain"
security set-keychain-settings -lut 21600 "$keychain"
security unlock-keychain -p "$keychain_password" "$keychain"
security import "$work/certificate.p12" -k "$keychain" -P "$MACOS_CERTIFICATE_PASSWORD" -T /usr/bin/codesign
security set-key-partition-list -S apple-tool:,apple:,codesign: -s -k "$keychain_password" "$keychain"

while IFS= read -r -d '' file; do
  if [[ "$file" == *.dylib || "$file" == *.so ]] || file "$file" | grep -Eq 'Mach-O'; then
    codesign --force --options runtime --timestamp --sign "$MACOS_SIGNING_IDENTITY" --keychain "$keychain" "$file"
  fi
done < <(find "$app" -type f \( -perm -111 -o -name '*.dylib' -o -name '*.so' \) -print0)
while IFS= read -r -d '' bundle; do
  [[ "$bundle" == "$app" ]] && continue
  codesign --force --options runtime --timestamp --sign "$MACOS_SIGNING_IDENTITY" --keychain "$keychain" "$bundle"
done < <(find "$app" -depth -type d \( -name '*.framework' -o -name '*.app' -o -name '*.xpc' \) -print0)
codesign --force --options runtime --timestamp --sign "$MACOS_SIGNING_IDENTITY" --keychain "$keychain" "$app"
codesign --verify --strict --deep --verbose=2 "$app"
