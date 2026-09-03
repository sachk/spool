#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
# shellcheck source=tools/lib/manifest-sources.sh
source "$ROOT/tools/lib/manifest-sources.sh"
ABI="${ANDROID_ABI:-x86_64}"
QT_VERSION="${QT_VERSION:-$(toolchain_field "$ROOT" qt.version)}"
QT_PREFIX="${QT_ANDROID_PREFIX:-$ROOT/build/android/qt/$QT_VERSION/android_${ABI//-/_}}"
DEPS_PREFIX="${ANDROID_DEPS_PREFIX:-$ROOT/build/android/deps/$ABI}"
JOBS="${ANDROID_BUILD_JOBS:-$(nproc)}"
# The Nix setup hooks export every host build input through these variables, and
# Qt's Android toolchain folds QT_ADDITIONAL_PACKAGES_PREFIX_PATH into
# CMAKE_FIND_ROOT_PATH, which androiddeployqt then scans for libraries to bundle.
# Left alone, host GL/X11 libraries end up in the APK. Keep Android prefixes only.
unset CMAKE_PREFIX_PATH CMAKE_INCLUDE_PATH CMAKE_LIBRARY_PATH CMAKE_FRAMEWORK_PATH \
  Qt6_DIR QT_ADDITIONAL_PACKAGES_PREFIX_PATH QT_ADDITIONAL_HOST_PACKAGES_PREFIX_PATH \
  QMAKEPATH QML2_IMPORT_PATH QT_PLUGIN_PATH PKG_CONFIG_PATH
# Qt's Android toolchain folds this into both CMAKE_PREFIX_PATH and
# CMAKE_FIND_ROOT_PATH, so cross-compiled packages resolve despite the ONLY find
# root mode. It is also what androiddeployqt scans, so it must list Android
# prefixes and nothing else.
export QT_ADDITIONAL_PACKAGES_PREFIX_PATH="$ROOT/build/android/qcoro/$ABI:$DEPS_PREFIX"
# nixpkgs patches CMake's Unix platform module to drop /usr from the default
# search prefixes, which also stops find_path from looking in the NDK sysroot's
# /usr/include. Put it back so EGL/GLESv2 resolve against the NDK.
NDK_PREFIX_PATH=/usr

case "$ABI" in
  arm64-v8a | x86_64) ;;
  *)
    echo "error: unsupported Android ABI: $ABI" >&2
    exit 1
    ;;
esac

: "${ANDROID_HOME:?run through nix develop .#android}"
: "${ANDROID_NDK_ROOT:?run through nix develop .#android}"
[[ -x "$QT_PREFIX/bin/qt-cmake" ]] || {
  echo "error: Android Qt is not built at $QT_PREFIX" >&2
  exit 1
}
[[ -f "$DEPS_PREFIX/lib/libmpv.so" ]] || {
  echo "error: Android dependencies are not built at $DEPS_PREFIX" >&2
  exit 1
}

# androiddeployqt emits an unsigned release APK, so something has to sign it.
#
# The upload key is the one that matters: a build signed with anything else
# cannot be installed over the app already on a device, so a locally built APK
# that is not signed with it is not a build of the same app. Release jobs pass
# the key through the environment. Locally, a credentials file is read if there
# is one, so the real key is simply what a local build uses -- there is nothing
# to remember to set. Without it the build falls back to a debug key, which is
# installable on a clean device and nowhere else.
#
# The file is JSON, beside the keystore, and belongs outside the repository:
#
#   {"keystore": "/abs/path/spool-upload.p12", "alias": "...",
#    "storePassword": "...", "keyPassword": "..."}
SPOOL_ANDROID_SIGNING_CREDENTIALS="${SPOOL_ANDROID_SIGNING_CREDENTIALS:-${XDG_DATA_HOME:-$HOME/.local/share}/spool/signing/android-upload-credentials.json}"

prepare_keystore() {
  if [[ -z "${SPOOL_ANDROID_KEYSTORE_PATH:-}" && -f "$SPOOL_ANDROID_SIGNING_CREDENTIALS" ]]; then
    printf 'signing with the key named by %s\n' "$SPOOL_ANDROID_SIGNING_CREDENTIALS"
    local field
    while IFS=$'\t' read -r field value; do
      case "$field" in
        keystore) SPOOL_ANDROID_KEYSTORE_PATH="$value" ;;
        alias) SPOOL_ANDROID_KEYSTORE_ALIAS="$value" ;;
        storePassword) SPOOL_ANDROID_KEYSTORE_STORE_PASS="$value" ;;
        keyPassword) SPOOL_ANDROID_KEYSTORE_KEY_PASS="$value" ;;
      esac
    done < <(python3 -c '
import json
import sys

with open(sys.argv[1], encoding="utf-8") as handle:
    for field, value in json.load(handle).items():
        print(field, value, sep="\t")
' "$SPOOL_ANDROID_SIGNING_CREDENTIALS")
  fi
  if [[ -n "${SPOOL_ANDROID_KEYSTORE_PATH:-}" ]]; then
    [[ -f "$SPOOL_ANDROID_KEYSTORE_PATH" ]] || {
      echo "error: release keystore is missing at $SPOOL_ANDROID_KEYSTORE_PATH" >&2
      exit 1
    }
    : "${SPOOL_ANDROID_KEYSTORE_ALIAS:?release keystore alias is required}"
    : "${SPOOL_ANDROID_KEYSTORE_STORE_PASS:?release keystore password is required}"
    : "${SPOOL_ANDROID_KEYSTORE_KEY_PASS:?release key password is required}"
    export QT_ANDROID_KEYSTORE_PATH="$SPOOL_ANDROID_KEYSTORE_PATH"
    export QT_ANDROID_KEYSTORE_ALIAS="$SPOOL_ANDROID_KEYSTORE_ALIAS"
    export QT_ANDROID_KEYSTORE_STORE_PASS="$SPOOL_ANDROID_KEYSTORE_STORE_PASS"
    export QT_ANDROID_KEYSTORE_KEY_PASS="$SPOOL_ANDROID_KEYSTORE_KEY_PASS"
    return
  fi

  printf 'no signing key configured (%s); signing with a debug key, which will not install over an existing build\n' \
    "$SPOOL_ANDROID_SIGNING_CREDENTIALS" >&2
  local keystore="$ROOT/build/android/debug.keystore"
  if [[ ! -f "$keystore" ]]; then
    mkdir -p "$(dirname "$keystore")"
    keytool -genkeypair -keystore "$keystore" -alias spooldebug \
      -storepass spooldebug -keypass spooldebug \
      -keyalg RSA -keysize 2048 -validity 10000 \
      -dname "CN=Spool Debug, OU=Spool, O=Spool, L=None, ST=None, C=AU"
  fi
  export QT_ANDROID_KEYSTORE_PATH="$keystore"
  export QT_ANDROID_KEYSTORE_ALIAS=spooldebug
  export QT_ANDROID_KEYSTORE_STORE_PASS=spooldebug
  export QT_ANDROID_KEYSTORE_KEY_PASS=spooldebug
}

build_qcoro() {
  local build="$ROOT/build/android/qcoro-build/$ABI"
  local prefix="$ROOT/build/android/qcoro/$ABI"
  if [[ -f "$prefix/lib/cmake/QCoro6/QCoro6Config.cmake" ]]; then
    printf 'Android QCoro for %s is current\n' "$ABI"
    return
  fi
  rm -rf "$build" "$prefix"
  "$QT_PREFIX/bin/qt-cmake" -S "$ROOT/build/android/sources/third-party/qcoro" -B "$build" -GNinja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_SYSTEM_PREFIX_PATH="$NDK_PREFIX_PATH" \
    -DCMAKE_INSTALL_PREFIX="$prefix" \
    -DQt6_DIR="$QT_PREFIX/lib/cmake/Qt6" \
    -DQt6Core_DIR="$QT_PREFIX/lib/cmake/Qt6Core" \
    -DQt6CorePrivate_DIR="$QT_PREFIX/lib/cmake/Qt6CorePrivate" \
    -DQt6Network_DIR="$QT_PREFIX/lib/cmake/Qt6Network" \
    -DQT_HOST_PATH="${SPOOL_ANDROID_QT_HOST:-}" \
    -DQT_HOST_PATH_CMAKE_DIR="${SPOOL_ANDROID_QT_HOST:-}/lib/cmake" \
    -DQCORO_BUILD_EXAMPLES=OFF \
    -DQCORO_WITH_QTDBUS=OFF \
    -DQCORO_WITH_QML=OFF \
    -DQCORO_WITH_QTQUICK=OFF \
    -DQCORO_WITH_QTWEBSOCKETS=OFF \
    -DQCORO_BUILD_TESTING=OFF \
    -DBUILD_TESTING=OFF
  cmake --build "$build" --parallel "$JOBS"
  cmake --install "$build"
}

build_variant() {
  local variant="$1"
  local tv=OFF
  [[ "$variant" == tv ]] && tv=ON
  local build="$ROOT/build/android/app-$variant-$ABI"
  rm -rf "$build"
  "$QT_PREFIX/bin/qt-cmake" -S "$ROOT" -B "$build" -GNinja \
    -DQt6_DIR="$QT_PREFIX/lib/cmake/Qt6" \
    -DQt6Core_DIR="$QT_PREFIX/lib/cmake/Qt6Core" \
    -DQt6Gui_DIR="$QT_PREFIX/lib/cmake/Qt6Gui" \
    -DQt6Network_DIR="$QT_PREFIX/lib/cmake/Qt6Network" \
    -DQt6OpenGL_DIR="$QT_PREFIX/lib/cmake/Qt6OpenGL" \
    -DQt6Qml_DIR="$QT_PREFIX/lib/cmake/Qt6Qml" \
    -DQt6Quick_DIR="$QT_PREFIX/lib/cmake/Qt6Quick" \
    -DQt6Sql_DIR="$QT_PREFIX/lib/cmake/Qt6Sql" \
    -DQt6Svg_DIR="$QT_PREFIX/lib/cmake/Qt6Svg" \
    -DQt6WebSockets_DIR="$QT_PREFIX/lib/cmake/Qt6WebSockets" \
    -DQt6LinguistTools_DIR="${SPOOL_ANDROID_QT_HOST:-}/lib/cmake/Qt6LinguistTools" \
    -DQT_HOST_PATH="${SPOOL_ANDROID_QT_HOST:-}" \
    -DQT_HOST_PATH_CMAKE_DIR="${SPOOL_ANDROID_QT_HOST:-}/lib/cmake" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_SYSTEM_PREFIX_PATH="$NDK_PREFIX_PATH" \
    -DBUILD_TESTING=OFF \
    -DJELLYFIN_NATIVE_WEBOS=OFF \
    -DSPOOL_ANDROID_TV="$tv" \
    -DANDROID_ABI="$ABI" \
    -DANDROID_PLATFORM=android-28 \
    -DANDROID_DEPS_PREFIX="$DEPS_PREFIX" \
    -DQT_ANDROID_SIGN_APK=ON
  cmake --build "$build" --target jellyfin-native_make_apk --parallel "$JOBS"

  local apk="$build/android-build/jellyfin-native.apk"
  [[ -f "$apk" ]] || {
    echo "error: $variant APK was not generated at $apk" >&2
    exit 1
  }
  mkdir -p "$ROOT/dist/android"
  cp -f "$apk" "$ROOT/dist/android/spool-${variant}-${ABI}.apk"
}

prepare_keystore
build_qcoro
# Both variants share every expensive input and differ only in manifest and TV
# policy. Building them together avoids duplicate toolchain work; CI uses this
# same invocation rather than independent phone and television jobs. Name a
# subset in SPOOL_ANDROID_VARIANTS when only one is wanted.
read -r -a VARIANTS <<<"${SPOOL_ANDROID_VARIANTS:-phone tv}"
for variant in "${VARIANTS[@]}"; do
  case "$variant" in
    phone | tv) build_variant "$variant" ;;
    *)
      echo "error: unknown Android variant: $variant" >&2
      exit 1
      ;;
  esac
done
printf 'Android APKs:\n'
for variant in "${VARIANTS[@]}"; do
  printf '  %s\n' "$ROOT/dist/android/spool-${variant}-${ABI}.apk"
done
