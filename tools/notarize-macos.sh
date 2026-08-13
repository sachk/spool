#!/usr/bin/env bash
set -euo pipefail

dmg="${1:?usage: notarize-macos.sh DMG}"
: "${APPLE_NOTARY_KEY_P8:?APPLE_NOTARY_KEY_P8 is required}"
: "${APPLE_NOTARY_KEY_ID:?APPLE_NOTARY_KEY_ID is required}"
: "${APPLE_NOTARY_ISSUER_ID:?APPLE_NOTARY_ISSUER_ID is required}"
[[ -f "$dmg" ]] || { printf 'DMG not found: %s\n' "$dmg" >&2; exit 1; }

work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
key="$work/AuthKey_${APPLE_NOTARY_KEY_ID}.p8"
printf '%s' "$APPLE_NOTARY_KEY_P8" >"$key"
chmod 600 "$key"
result="$work/notary-result.json"
printf 'Submitting %s to Apple notarization (15-minute limit)\n' "$dmg"
xcrun notarytool submit "$dmg" --wait --timeout 15m --verbose --output-format json \
  --key "$key" --key-id "$APPLE_NOTARY_KEY_ID" --issuer "$APPLE_NOTARY_ISSUER_ID" >"$result"
cat "$result"
status="$(plutil -extract status raw -o - "$result")"
submission_id="$(plutil -extract id raw -o - "$result")"
if [[ "$status" != Accepted ]]; then
  xcrun notarytool log "$submission_id" --key "$key" --key-id "$APPLE_NOTARY_KEY_ID" \
    --issuer "$APPLE_NOTARY_ISSUER_ID" || true
  exit 1
fi
xcrun stapler staple "$dmg"
xcrun stapler validate "$dmg"
spctl --assess --type open --context context:primary-signature --verbose=2 "$dmg"
