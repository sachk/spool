#!/usr/bin/env bash
# Merge the per-ABI APKs into one that installs on any device.
#
#   tools/android/build-universal-apk.sh OUT.apk IN.apk IN.apk [...]
#
# The release page offers this so somebody who does not know their device's
# architecture still downloads something that installs. The in-app updater
# never fetches it: the update manifest is keyed by ABI and this file is not in
# it, so an installed build only ever updates to the APK built for its own ABI.
#
# Every input is the same application built for a different architecture, so
# the merge is a union of their lib/ directories over one shared payload. That
# only holds if the shared payload really is shared, which is checked rather
# than assumed -- a version code that moved between two matrix legs would
# otherwise be silently resolved in favour of whichever ran first.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
# shellcheck source=tools/android/signing.sh
source "$ROOT/tools/android/signing.sh"

[[ $# -ge 3 ]] || {
  echo "usage: $0 OUT.apk IN.apk IN.apk [...]" >&2
  exit 2
}
OUT="$1"
shift
for apk in "$@"; do
  [[ -f "$apk" ]] || {
    echo "error: input APK missing at $apk" >&2
    exit 1
  }
done

: "${ANDROID_HOME:?run through nix develop .#android}"
BUILD_TOOLS="${ANDROID_BUILD_TOOLS:-$ANDROID_HOME/build-tools/36.0.0}"
for tool in zipalign apksigner; do
  [[ -x "$BUILD_TOOLS/$tool" ]] || {
    echo "error: $tool missing at $BUILD_TOOLS/$tool" >&2
    exit 1
  }
done

work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
mkdir -p "$(dirname "$OUT")"

python3 "$ROOT/tools/android/merge_apks.py" --output "$work/unsigned.apk" "$@"

prepare_keystore
# Plain 4-byte alignment, not -p: the native libraries are deflated, as Qt
# packaged them, so there is nothing to map on a page boundary. resources.arsc
# is the entry the platform requires to be stored and aligned, and it is.
"$BUILD_TOOLS/zipalign" -f 4 "$work/unsigned.apk" "$work/aligned.apk"
"$BUILD_TOOLS/apksigner" sign \
  --ks "$QT_ANDROID_KEYSTORE_PATH" \
  --ks-key-alias "$QT_ANDROID_KEYSTORE_ALIAS" \
  --ks-pass "pass:$QT_ANDROID_KEYSTORE_STORE_PASS" \
  --key-pass "pass:$QT_ANDROID_KEYSTORE_KEY_PASS" \
  --out "$OUT" \
  "$work/aligned.apk"
"$BUILD_TOOLS/zipalign" -c 4 "$OUT"
"$BUILD_TOOLS/apksigner" verify --verbose --print-certs "$OUT"

printf 'universal APK: %s (%s)\n' "$OUT" "$(du -h "$OUT" | cut -f1)"
