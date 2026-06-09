#!/usr/bin/env bash
# Cross-compile the heaptrack *recorder* (preload lib + driver) for the webOS
# arm-webos-linux-gnueabi target. The device only records a raw trace; symbol
# resolution (heaptrack_interpret) and analysis (heaptrack_gui / heaptrack_print)
# run on the desktop from nixpkgs. heaptrack_interpret resolves the ARM binaries
# via --sysroot / --extra-paths (build-id matched), so we do NOT need elfutils
# (libdw) in the webOS sysroot, which it lacks.
#
# We build from the *same* source nixpkgs uses for the desktop heaptrack so the
# raw trace format is guaranteed compatible with the local heaptrack tools.
#
# Output (installed under $PREFIX, ready to drop into the app bundle):
#   bin/heaptrack                          driver script
#   lib/heaptrack/libheaptrack_preload.so  LD_PRELOAD recorder
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"

SDK_ROOT="${WEBOS_SDK_ROOT:-$ROOT/build/webos-sdk/arm-webos-linux-gnueabi_sdk-buildroot}"
TOOLCHAIN_FILE="$ROOT/tools/webos-native/qt6-webos-toolchain.cmake"

BUILD_ROOT="${HEAPTRACK_BUILD_ROOT:-$ROOT/build/webos-heaptrack}"
SRC_DIR="$BUILD_ROOT/src"
BUILD_DIR="$BUILD_ROOT/build"
PREFIX="${HEAPTRACK_PREFIX:-$BUILD_ROOT/install}"

if [[ ! -x "$SDK_ROOT/bin/arm-webos-linux-gnueabi-g++" ]]; then
  echo "error: webOS SDK compiler not found under $SDK_ROOT/bin" >&2
  exit 1
fi
export WEBOS_SDK_ROOT="$SDK_ROOT"

# Source: prefer an explicit checkout, otherwise reuse the nixpkgs source so the
# format matches the desktop heaptrack_gui exactly.
if [[ -n "${HEAPTRACK_SRC:-}" ]]; then
  upstream="$HEAPTRACK_SRC"
else
  echo "Resolving heaptrack source from nixpkgs (format-matched to desktop GUI)..."
  upstream="$(nix build nixpkgs#heaptrack.src --no-link --print-out-paths)"
fi

rm -rf "$SRC_DIR"
mkdir -p "$SRC_DIR" "$BUILD_DIR" "$PREFIX"
cp -a "$upstream"/. "$SRC_DIR"/
chmod -R u+w "$SRC_DIR"

# Configure: device recorder only. GUI/print/interpret are host-side and stay
# off (interpret needs elfutils/libdw, absent from the webOS sysroot — we run it
# on the desktop instead). zstd is absent from the sysroot so heaptrack falls
# back to zlib/gzip for the raw trace (fine, slightly bigger files).
cmake -S "$SRC_DIR" -B "$BUILD_DIR" -G "Unix Makefiles" \
  -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN_FILE" \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_INSTALL_PREFIX="$PREFIX" \
  -DHEAPTRACK_BUILD_GUI=OFF \
  -DHEAPTRACK_BUILD_PRINT=OFF \
  -DHEAPTRACK_BUILD_TRACK=ON \
  -DHEAPTRACK_BUILD_INTERPRET=OFF \
  -DHEAPTRACK_USE_LIBUNWIND=OFF

# With GUI/print/interpret off this builds only the recorder pieces
# (libheaptrack_preload.so + heaptrack_env helper + the driver script).
cmake --build "$BUILD_DIR" -j"$(nproc)"
cmake --install "$BUILD_DIR"

preload="$PREFIX/lib/heaptrack/libheaptrack_preload.so"
driver="$PREFIX/bin/heaptrack"
if [[ ! -f "$preload" || ! -f "$driver" ]]; then
  echo "error: expected heaptrack artifacts missing after install" >&2
  echo "  preload: $preload" >&2
  echo "  driver:  $driver" >&2
  exit 1
fi

echo
echo "heaptrack recorder built for webOS:"
file "$preload" 2>/dev/null || true
echo "Installed under: $PREFIX"
