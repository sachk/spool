#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
if [[ -n "${WEBOS_SDK_ROOT:-}" ]]; then
  SDK_ROOT="$WEBOS_SDK_ROOT"
elif [[ -d "$ROOT/../build/webos-sdk/arm-webos-linux-gnueabi_sdk-buildroot" ]]; then
  SDK_ROOT="$ROOT/../build/webos-sdk/arm-webos-linux-gnueabi_sdk-buildroot"
else
  SDK_ROOT="$ROOT/build/webos-sdk/arm-webos-linux-gnueabi_sdk-buildroot"
fi
SYSROOT="$SDK_ROOT/arm-webos-linux-gnueabi/sysroot"

export PKG_CONFIG_SYSROOT_DIR="$SYSROOT"
export PKG_CONFIG_LIBDIR="$SYSROOT/usr/lib/pkgconfig:$SYSROOT/usr/share/pkgconfig"
export PKG_CONFIG_PATH=""

exec pkg-config "$@"
