#!/usr/bin/env bash
# Both APKs, from nothing, in one command:
#
#   nix develop .#android -c bash tools/android/build.sh
#
# ANDROID_ABI selects the ABI (x86_64 for the emulator, arm64-v8a or
# armeabi-v7a for a device). Each stage is cached, so a repeat run only rebuilds
# what moved.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"

bash "$ROOT/tools/android/build-dependencies.sh"
bash "$ROOT/tools/android/build-qt6.sh"
bash "$ROOT/tools/android/build-apks.sh"
