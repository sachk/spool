#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
PHONE_APK="${1:-$ROOT/dist/android/spool-phone-x86_64.apk}"
TV_APK="${2:-$ROOT/dist/android/spool-tv-x86_64.apk}"
EMULATOR_LOG="${ANDROID_EMULATOR_LOG:-$ROOT/build/android/emulator.log}"
ARTIFACT_DIR="${ANDROID_LAUNCH_TEST_DIR:-$ROOT/build/android/launch-test}"
# Both variants launch through the app's own QtActivity subclass, which owns the
# launch screen's exit. Naming it here is what catches a manifest that fell back
# to Qt's stock activity and dropped that handover.
SPOOL_ACTIVITY=com.sachk.spool.SpoolActivity

: "${ANDROID_HOME:?run through nix develop .#android}"
ADB="$ANDROID_HOME/platform-tools/adb"
[[ -x "$ADB" ]] || {
  echo "error: adb missing at $ADB" >&2
  exit 1
}
[[ -f "$PHONE_APK" ]] || {
  echo "error: phone APK missing at $PHONE_APK" >&2
  exit 1
}
[[ -f "$TV_APK" ]] || {
  echo "error: TV APK missing at $TV_APK" >&2
  exit 1
}

cleanup() {
  "$ADB" emu kill >/dev/null 2>&1 || true
}
trap cleanup EXIT

mkdir -p "$(dirname "$EMULATOR_LOG")" "$ARTIFACT_DIR"
: >"$EMULATOR_LOG"
nix run "$ROOT#android-emulator" >"$EMULATOR_LOG" 2>&1 &
for _ in $(seq 1 30); do
  ANDROID_SERIAL="$("$ADB" devices | sed -n 's/^\(emulator-[0-9]*\)[[:space:]].*/\1/p' | sed -n '1p')"
  [[ -n "$ANDROID_SERIAL" ]] && break
  sleep 1
done
[[ -n "${ANDROID_SERIAL:-}" ]] || {
  cat "$EMULATOR_LOG" >&2
  echo "error: Android emulator did not register with adb" >&2
  exit 1
}
export ANDROID_SERIAL

for _ in $(seq 1 90); do
  if "$ADB" wait-for-device >/dev/null 2>&1 &&
    [[ "$("$ADB" shell getprop sys.boot_completed 2>/dev/null | tr -d '\r')" == 1 ]]; then
    break
  fi
  sleep 2
done
[[ "$("$ADB" shell getprop sys.boot_completed 2>/dev/null | tr -d '\r')" == 1 ]] || {
  cat "$EMULATOR_LOG" >&2
  echo "error: Android emulator did not boot" >&2
  exit 1
}

# Resolves through the launcher category the variant actually advertises, so a
# TV package that never registered a leanback entry point fails here rather
# than passing on the activity class name alone.
launcher_component() {
  local package="$1" category="$2"
  "$ADB" shell cmd package resolve-activity --brief \
    -a android.intent.action.MAIN -c "$category" "$package" |
    tr -d '\r' | sed -n "s#^\($package/[A-Za-z0-9_.\$]*\)\$#\1#p" | sed -n '1p'
}

fail() {
  local package="$1"
  shift
  "$ADB" logcat -d >"$ARTIFACT_DIR/$package-logcat.txt" 2>/dev/null || true
  "$ADB" exec-out screencap -p >"$ARTIFACT_DIR/$package-screen.png" 2>/dev/null || true
  sed -n '1,200p' "$ARTIFACT_DIR/$package-logcat.txt" >&2 || true
  echo "error: $*" >&2
  echo "note: full logcat and screenshot under $ARTIFACT_DIR" >&2
  exit 1
}

launch_apk() {
  local apk="$1" package="$2" category="$3" component activity
  "$ADB" install -r "$apk"
  component="$(launcher_component "$package" "$category")"
  [[ -n "$component" ]] || fail "$package" "$package advertises no $category launcher activity"
  # resolve-activity abbreviates a class that sits under the package's own
  # namespace, so the phone package reports .SpoolActivity where the TV package,
  # whose name it does not share, reports the class in full.
  activity="${component#*/}"
  if [[ "$activity" == .* ]]; then
    activity="$package$activity"
  fi
  [[ "$activity" == "$SPOOL_ACTIVITY" ]] ||
    fail "$package" "$package launcher is $activity, expected $SPOOL_ACTIVITY"

  "$ADB" shell am force-stop "$package"
  "$ADB" logcat -c
  "$ADB" shell am start -W -n "$component"
  for _ in $(seq 1 30); do
    "$ADB" logcat -d -s Spool:V | grep -qF 'startup: QML source loaded' && break
    sleep 1
  done

  [[ -n "$("$ADB" shell pidof "$package" | tr -d '\r')" ]] ||
    fail "$package" "$package exited during launch"
  "$ADB" shell dumpsys activity activities | grep -F "$component" >/dev/null ||
    fail "$package" "$package activity is not present"
  "$ADB" logcat -d -s Spool:V | grep -qF 'startup: QML source loaded' ||
    fail "$package" "$package did not load its QML scene"
  ! "$ADB" logcat -d -s Spool:V | grep -qE '\[qml\] |\[qt:(crit|fatal)\] ' ||
    fail "$package" "$package reported QML errors"
  ! "$ADB" logcat -d -b crash -b main | grep -qE 'FATAL EXCEPTION|Fatal signal' ||
    fail "$package" "Android reported a fatal launch failure for $package"

  "$ADB" exec-out screencap -p >"$ARTIFACT_DIR/$package-screen.png"
  "$ADB" shell am force-stop "$package"
  printf '%s launched; screenshot at %s\n' "$package" "$ARTIFACT_DIR/$package-screen.png"
}

launch_apk "$PHONE_APK" com.sachk.spool android.intent.category.LAUNCHER
launch_apk "$TV_APK" com.sachk.spool.tv android.intent.category.LEANBACK_LAUNCHER
printf 'phone and Android TV APK launch tests passed\n'
