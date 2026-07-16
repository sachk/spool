#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

if [[ -z "${IN_NIX_SHELL:-}" && "${JELLYFIN_BUILD_IPK_ENTERED_NIX:-0}" != "1" ]]; then
  WORKSPACE_ROOT="$(cd "$ROOT/.." && pwd)"
  DEFAULT_SDK_ROOT="$ROOT/build/webos-sdk/arm-webos-linux-gnueabi_sdk-buildroot"
  if [[ -z "${WEBOS_SDK_ROOT:-}" && -d "$WORKSPACE_ROOT/build/webos-sdk/arm-webos-linux-gnueabi_sdk-buildroot" ]]; then
    DEFAULT_SDK_ROOT="$WORKSPACE_ROOT/build/webos-sdk/arm-webos-linux-gnueabi_sdk-buildroot"
  fi
  cd "$ROOT"
  exec nix develop "$ROOT" -c env \
    JELLYFIN_BUILD_IPK_ENTERED_NIX=1 \
    WEBOS_SDK_ROOT="${WEBOS_SDK_ROOT:-$DEFAULT_SDK_ROOT}" \
    bash -c 'cd "$1" && shift && exec bash ./build-ipk.sh "$@"' bash "$ROOT" "$@"
fi

WEBOS_TOOLS_ROOT="${WEBOS_TOOLS_ROOT:-$ROOT/tools/webos-native}"
# shellcheck source=tools/lib/build-common.sh
source "$ROOT/tools/lib/build-common.sh"
# shellcheck source=tools/webos-native/nixos-sdk-compat.sh
source "$ROOT/tools/webos-native/nixos-sdk-compat.sh"
PHASE="${1:-all}"
DO_BUILD=0
DO_STAGE=0
DO_PACKAGE=0
case "$PHASE" in
  app) DO_BUILD=1 ;;
  stage) DO_STAGE=1 ;;
  package) DO_PACKAGE=1 ;;
  all)
    DO_BUILD=1
    DO_STAGE=1
    DO_PACKAGE=1
    ;;
  *)
    echo "usage: $0 [app|stage|package|all]" >&2
    exit 2
    ;;
esac

SDK_ROOT="${WEBOS_SDK_ROOT:-$ROOT/build/webos-sdk/arm-webos-linux-gnueabi_sdk-buildroot}"
SYSROOT="$SDK_ROOT/arm-webos-linux-gnueabi/sysroot"
PREFIX="$SYSROOT/usr/local/webos-native"
MPV_SRC="${MPV_SRC:-$ROOT/mpv}"
MPV_BUILD="${MPV_BUILD:-$MPV_SRC/build/webos-libmpv}"
WEBOS_CROSS_FILE="${WEBOS_CROSS_FILE:-$ROOT/build/webos-thirdparty-build/webos.cross.ini}"
QT6_HOST_PREFIX="${QT6_HOST_PREFIX:-$ROOT/build/qt6-611-host-install}"
APP_SOURCE_DIR="$ROOT/app"
BUILD_DIR="$ROOT/build"
CMAKE_BUILD_DIR="${WEBOS_APP_BUILD_DIR:-$ROOT/build/webos/app}"
INSTALL_DIR="$CMAKE_BUILD_DIR/install"
APP_DIR="${WEBOS_STAGE_DIR:-$ROOT/build/webos/stage/app}"
STAGE_LIB="$APP_DIR/lib"
STAGE_BIN="$APP_DIR/bin"
STRIP_BIN="$SDK_ROOT/bin/arm-webos-linux-gnueabi-strip"
READELF_BIN="${WEBOS_READELF_BIN:-$SDK_ROOT/bin/arm-webos-linux-gnueabi-readelf}"
GCC_AR_BIN="$SDK_ROOT/bin/arm-webos-linux-gnueabi-gcc-ar"
GCC_RANLIB_BIN="$SDK_ROOT/bin/arm-webos-linux-gnueabi-gcc-ranlib"
MPV_LTO_CROSS_FILE="$BUILD_DIR/webos-lto.cross.ini"
WEBOS_BUILD_MEMORY_PER_JOB_MIB="${WEBOS_BUILD_MEMORY_PER_JOB_MIB:-1536}"
WEBOS_BUILD_MEMORY_RESERVE_MIB="${WEBOS_BUILD_MEMORY_RESERVE_MIB:-2048}"
WEBOS_BUILD_JOBS="$(recommended_parallel_jobs "$WEBOS_BUILD_MEMORY_PER_JOB_MIB" "$WEBOS_BUILD_MEMORY_RESERVE_MIB")"

QT6_PREFIX="${QT6_PREFIX:-$ROOT/build/qt6-611-target-static-install}"
if (( DO_BUILD || DO_STAGE )); then
  ensure_webos_sdk_host_tools "$SDK_ROOT"

  if [[ ! -f "$QT6_PREFIX/lib/libQt6Core.a" ]]; then
    echo "error: static Qt6 install not found at $QT6_PREFIX" >&2
    echo "       Run: QT_STATIC=1 bash $WEBOS_TOOLS_ROOT/build-qt6-611.sh target" >&2
    exit 1
  fi
  echo "Using static Qt6 build at $QT6_PREFIX"

  # Reject stale cached Qt prefixes that predate required local patches.
  QT_PATCH_MARKERS=(JELLYFIN_QT_NO_CURSOR_SURFACE)
  for marker in "${QT_PATCH_MARKERS[@]}"; do
    if ! grep -rqal "$marker" "$QT6_PREFIX/lib" 2>/dev/null; then
      echo "error: Qt at $QT6_PREFIX is missing patch marker '$marker'." >&2
      echo "       Rerun: bash $WEBOS_TOOLS_ROOT/build-qt6-611.sh" >&2
      exit 1
    fi
  done

  #QT_GUI_CONFIG="$QT6_PREFIX/include/QtGui/qtgui-config.h"
  #if ! grep -q '^#define QT_FEATURE_accessibility 1$' "$QT_GUI_CONFIG" 2>/dev/null; then
  #  echo "error: Qt at $QT6_PREFIX was built without accessibility support." >&2
  #  echo "       Rerun: bash $WEBOS_TOOLS_ROOT/build-qt6-611.sh" >&2
  #  exit 1
  #fi

  if [[ ! -f "$QT6_PREFIX/lib/cmake/QCoro6/QCoro6Config.cmake" ]]; then
    echo "error: target QCoro is missing from $QT6_PREFIX." >&2
    echo "       Run: bash $WEBOS_TOOLS_ROOT/build-qcoro.sh" >&2
    exit 1
  fi
fi

if (( DO_BUILD )); then
describe_parallel_jobs "$WEBOS_BUILD_JOBS" "webOS app" "$WEBOS_BUILD_MEMORY_PER_JOB_MIB" "$WEBOS_BUILD_MEMORY_RESERVE_MIB"

DOVI_TOOL_ROOT="${DOVI_TOOL_ROOT:-$ROOT/build/third_party/dovi_tool}"
if [[ ! -f "$DOVI_TOOL_ROOT/dolby_vision/Cargo.toml" ]]; then
  echo "error: verified dovi_tool source not found at $DOVI_TOOL_ROOT" >&2
  echo "       Run: bash tools/webos-native/build-third-party.sh fetch" >&2
  exit 1
fi

if [[ ! -f "$PREFIX/lib/libcurl.a" ]]; then
  echo "error: private static curl is missing from $PREFIX." >&2
  echo "       Run: bash $WEBOS_TOOLS_ROOT/build-curl.sh build" >&2
  exit 1
fi

echo "Building libdovi..."
(
  cd "$DOVI_TOOL_ROOT/dolby_vision"
  DOVI_RUST_TOOLCHAIN="${DOVI_RUST_TOOLCHAIN:-1.89.0}"
  DOVI_RUST_TARGET="arm-unknown-linux-gnueabi"
  export CARGO_TARGET_ARM_UNKNOWN_LINUX_GNUEABI_LINKER="$SDK_ROOT/bin/arm-webos-linux-gnueabi-gcc"
  export CC_arm_unknown_linux_gnueabi="$SDK_ROOT/bin/arm-webos-linux-gnueabi-gcc"
  export CXX_arm_unknown_linux_gnueabi="$SDK_ROOT/bin/arm-webos-linux-gnueabi-g++"
  export AR_arm_unknown_linux_gnueabi="$SDK_ROOT/bin/arm-webos-linux-gnueabi-ar"
  export CARGO_BUILD_JOBS="${CARGO_BUILD_JOBS:-$WEBOS_BUILD_JOBS}"

  CARGO_CMD=(cargo)
  if command -v rustup >/dev/null 2>&1; then
    rustup toolchain install "$DOVI_RUST_TOOLCHAIN" \
      --profile minimal \
      --target "$DOVI_RUST_TARGET"
    CARGO_CMD=(rustup run "$DOVI_RUST_TOOLCHAIN" cargo)
  fi
  "${CARGO_CMD[@]}" build \
    --release \
    --features capi,serde \
    --target "$DOVI_RUST_TARGET" \
    --jobs "$WEBOS_BUILD_JOBS"
)
DOVI_LIB="$DOVI_TOOL_ROOT/dolby_vision/target/arm-unknown-linux-gnueabi/release/libdovi.a"
DOVI_INC="$DOVI_TOOL_ROOT/dolby_vision/include"

# Heaptrack profiling needs unwind tables and frame pointers for useful stacks,
# but release builds keep them out unless profiling is explicitly requested.
if [[ -z "${HEAPTRACK_UNWIND_FLAGS+x}" ]]; then
  if [[ "${BUNDLE_HEAPTRACK:-0}" == "1" ]]; then
    HEAPTRACK_UNWIND_FLAGS="-fasynchronous-unwind-tables -funwind-tables -fno-omit-frame-pointer -g"
  else
    HEAPTRACK_UNWIND_FLAGS=""
  fi
fi
WEBOS_TUNE_CFLAGS_EXPANDED="$(webos_tune_cflags)"
MPV_PGO_FLAGS="$(webos_pgo_flags MPV "$BUILD_DIR/pgo/mpv")"
APP_PGO_FLAGS="$(webos_pgo_flags APP "$BUILD_DIR/pgo/app")"
COMMON_WEBOS_CFLAGS="-I$DOVI_INC $WEBOS_TUNE_CFLAGS_EXPANDED $HEAPTRACK_UNWIND_FLAGS"
export CFLAGS="${CFLAGS:-} $COMMON_WEBOS_CFLAGS"
export CXXFLAGS="${CXXFLAGS:-} $COMMON_WEBOS_CFLAGS"
export LDFLAGS="${LDFLAGS:-} -L$(dirname "$DOVI_LIB")"
export PKG_CONFIG_PATH="$PREFIX/lib/pkgconfig"
export PKG_CONFIG_LIBDIR="$PREFIX/lib/pkgconfig:$SYSROOT/usr/lib/pkgconfig:$SYSROOT/usr/share/pkgconfig"
export PKG_CONFIG_SYSROOT_DIR="$SYSROOT"
# Host desktop sessions commonly export QML import paths for the system Qt/KDE
# stack. The webOS build must scan only the cross-built Qt prefix supplied below;
# otherwise qmlimportscanner can wander into Qt source/test fixtures and print
# parser errors for intentionally invalid QML files.
unset QML_IMPORT_PATH QML2_IMPORT_PATH NIXPKGS_QT6_QML_IMPORT_PATH

if [[ ! -x "$GCC_AR_BIN" || ! -x "$GCC_RANLIB_BIN" ]]; then
  echo "error: gcc-ar/gcc-ranlib are required for mpv LTO under $SDK_ROOT/bin" >&2
  exit 1
fi
mkdir -p "$(dirname "$MPV_LTO_CROSS_FILE")"
cat > "$MPV_LTO_CROSS_FILE" <<EOF
[binaries]
ar = '$GCC_AR_BIN'
ranlib = '$GCC_RANLIB_BIN'
EOF

MPV_SETUP_ARGS=(
  --cross-file "$WEBOS_CROSS_FILE"
  --cross-file "$MPV_LTO_CROSS_FILE"
  --prefix /usr/local
  --libdir lib
  --buildtype release
  -Db_lto=true
  -Dcplayer=false
  -Dlibmpv=true
  -Dlibcurl=enabled
  -Dtests=false
  -Dfuzzers=false
  -Dmanpage-build=disabled
  -Dhtml-build=disabled
  -Dpdf-build=disabled
  -Djavascript=disabled
  -Dlua=enabled
  -Dcplugins=disabled
  -Dlibarchive=disabled
  -Dlibavdevice=disabled
  -Ddvdnav=disabled
  -Dlibbluray=disabled
  -Dcdda=disabled
  -Ddvbin=disabled
  -Drubberband=disabled
  -Duchardet=disabled
  -Dvapoursynth=disabled
  -Dzimg=disabled
  -Djack=disabled
  -Dalsa=enabled
  -Doss-audio=disabled
  -Dpipewire=disabled
  -Dpulse=disabled
  -Dsdl2-gamepad=disabled
  -Dsdl2-video=disabled
  -Dx11=disabled
  -Dwayland=enabled
  -Dgl=disabled
  -Dplain-gl=disabled
  -Dvulkan=disabled
  -Dshaderc=disabled
  -Dspirv-cross=disabled
  -Djpeg=disabled
  -Dlcms2=disabled
  -Dzlib=enabled
  -Dstarfish=enabled
  "-Dc_link_args=-L$(dirname "$DOVI_LIB") -ldovi $MPV_PGO_FLAGS"
  "-Dcpp_link_args=-L$(dirname "$DOVI_LIB") -ldovi $MPV_PGO_FLAGS"
  # Explicit c_args/cpp_args: meson --reconfigure does not re-read CFLAGS env,
  # so pass the tuning + opt-in heaptrack/PGO flags here too.
  "-Dc_args=$WEBOS_TUNE_CFLAGS_EXPANDED $HEAPTRACK_UNWIND_FLAGS $MPV_PGO_FLAGS"
  "-Dcpp_args=$WEBOS_TUNE_CFLAGS_EXPANDED $HEAPTRACK_UNWIND_FLAGS $MPV_PGO_FLAGS"
)

if [[ -f "$MPV_BUILD/build.ninja" ]]; then
  meson setup --reconfigure "$MPV_BUILD" "$MPV_SRC" "${MPV_SETUP_ARGS[@]}"
else
  meson setup "$MPV_BUILD" "$MPV_SRC" "${MPV_SETUP_ARGS[@]}"
fi
meson compile -C "$MPV_BUILD" -j "$WEBOS_BUILD_JOBS"

if [[ -n "$APP_PGO_FLAGS" ]]; then
  export CFLAGS="$CFLAGS $APP_PGO_FLAGS"
  export CXXFLAGS="$CXXFLAGS $APP_PGO_FLAGS"
  export LDFLAGS="$LDFLAGS $APP_PGO_FLAGS"
fi

cmake -S "$ROOT" -B "$CMAKE_BUILD_DIR" -GNinja \
  --fresh \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_SHARED_LIBS=OFF \
  -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON \
  -DCMAKE_TOOLCHAIN_FILE="$WEBOS_TOOLS_ROOT/qt6-webos-toolchain.cmake" \
  -DCMAKE_STAGING_PREFIX="$QT6_PREFIX" \
  -DCMAKE_FIND_ROOT_PATH="$SYSROOT;$QT6_PREFIX" \
  -DCMAKE_PREFIX_PATH="$QT6_PREFIX;$QT6_PREFIX/lib/cmake" \
  -DQt6_DIR="$QT6_PREFIX/lib/cmake/Qt6" \
  -DQT_HOST_PATH="$QT6_HOST_PREFIX" \
  -DCMAKE_INSTALL_PREFIX=/usr/palm/applications/com.sachk.tern

cmake --build "$CMAKE_BUILD_DIR" --parallel "$WEBOS_BUILD_JOBS"
cmake --install "$CMAKE_BUILD_DIR" --prefix "$INSTALL_DIR"
fi

if (( DO_STAGE )); then
rm -rf "$APP_DIR"
mkdir -p "$BUILD_DIR" "$STAGE_LIB" "$STAGE_BIN" "$APP_DIR/qt-qml"
python3 "$ROOT/tools/render-appinfo.py" \
  "$APP_SOURCE_DIR/appinfo.json.in" \
  "$ROOT/VERSION" \
  "$APP_DIR/appinfo.json"
cp -f "$APP_SOURCE_DIR/icon.png" "$APP_DIR/icon.png"
mkdir -p "$APP_DIR/notices"
cp -f "$ROOT/app/notices/OPEN_SOURCE_NOTICES.txt" "$ROOT/LICENSE" \
  "$ROOT/qml/fonts/Inter-LICENSE.txt" "$ROOT/qml/fonts/MaterialIcons-LICENSE.txt" \
  "$ROOT/qml/fonts/SourceSerif4-LICENSE.md" "$APP_DIR/notices/"
PATCHELF_BIN="$(command -v patchelf)"

[[ -x "$INSTALL_DIR/bin/jellyfin-native" ]] || {
  echo "error: app install missing; run '$0 app' first" >&2
  exit 1
}
cp -f "$INSTALL_DIR/bin/jellyfin-native" "$STAGE_BIN/jellyfin-native"

# mpv + ffmpeg shared libs are always needed (not statically linked). Stage
# their real files and derive compatibility symlinks from each ELF SONAME so
# patch-level dependency upgrades do not require edits here.
MPV_STAGED_LIBRARY="$(stage_elf_shared_library \
  "$MPV_BUILD/libmpv.so.*" "$STAGE_LIB" "$READELF_BIN")"

# The Starfish build owns its HTTP/TLS stack. A dynamic dependency here would
# silently bind to arbitrary firmware ABIs and recreate playback-start crashes.
MPV_DYNAMIC_SECTION="$("$READELF_BIN" -d "$MPV_STAGED_LIBRARY")"
for network_library in libcurl libssl libcrypto; do
  if [[ "$MPV_DYNAMIC_SECTION" == *"[$network_library.so"* ]]; then
    echo "error: libmpv unexpectedly depends on firmware $network_library" >&2
    exit 1
  fi
done

for pattern in \
  "$PREFIX/lib/libavcodec.so.*" \
  "$PREFIX/lib/libavfilter.so.*" \
  "$PREFIX/lib/libavformat.so.*" \
  "$PREFIX/lib/libavutil.so.*" \
  "$PREFIX/lib/liblua5.2.so.*" \
  "$PREFIX/lib/libswresample.so.*" \
  "$PREFIX/lib/libswscale.so.*" \
  "$SYSROOT/usr/lib/libAcbAPI.so.*" \
  "$SYSROOT/usr/lib/libstdc++.so.*" \
  "$SYSROOT/lib/libgcc_s.so.*" \
  "$SYSROOT/usr/lib/libpcre2-16.so.*" \
  "$SYSROOT/usr/lib/libjpeg.so.*" \
  "$SYSROOT/usr/lib/libpng16.so.*"
do
  stage_elf_shared_library "$pattern" "$STAGE_LIB" "$READELF_BIN" >/dev/null
done

# patchelf on mpv (always needed)
"$PATCHELF_BIN" --force-rpath --set-rpath '$ORIGIN' "$MPV_STAGED_LIBRARY"
"$PATCHELF_BIN" --force-rpath --set-rpath '$ORIGIN/../lib' "$STAGE_BIN/jellyfin-native"

# strip binary to reduce size
"$STRIP_BIN" --strip-unneeded "$STAGE_BIN/jellyfin-native"

cat > "$STAGE_BIN/qt.conf" <<'QTCONF'
[Paths]
QmlImports = ../qt-qml
QTCONF

# --- Optional: bundle heaptrack memory recorder + launch shim --------------
# Controlled by BUNDLE_HEAPTRACK=1. Release IPKs keep main=bin/jellyfin-native
# as the real executable; the profiling shim is opt-in for development bundles.
HEAPTRACK_INSTALL="${HEAPTRACK_INSTALL:-$ROOT/build/webos-heaptrack/install}"
HEAPTRACK_SHIM="$ROOT/tools/webos/heaptrack-launch-shim.sh"
if [[ "${BUNDLE_HEAPTRACK:-0}" == "1" && -f "$HEAPTRACK_INSTALL/lib/heaptrack/libheaptrack_preload.so" ]]; then
  echo "Bundling heaptrack memory recorder + launch shim"
  mv -f "$STAGE_BIN/jellyfin-native" "$STAGE_BIN/jellyfin-native.real"
  install -m 0755 "$HEAPTRACK_SHIM" "$STAGE_BIN/jellyfin-native"
  mkdir -p "$STAGE_LIB/heaptrack/libexec"
  cp -f "$HEAPTRACK_INSTALL/lib/heaptrack/libheaptrack_preload.so" "$STAGE_LIB/heaptrack/"
  cp -f "$HEAPTRACK_INSTALL/bin/heaptrack" "$STAGE_BIN/heaptrack" 2>/dev/null || true
  cp -f "$HEAPTRACK_INSTALL/lib/heaptrack/libexec/heaptrack_env" "$STAGE_LIB/heaptrack/libexec/" 2>/dev/null || true
  # The preload (app/lib/heaptrack/) finds the bundled libstdc++ in app/lib via
  # $ORIGIN/.. ; the shim also exports LD_LIBRARY_PATH as a belt-and-suspenders.
  "$PATCHELF_BIN" --force-rpath --set-rpath '$ORIGIN/..' "$STAGE_LIB/heaptrack/libheaptrack_preload.so" 2>/dev/null || true
  # Preserve an unstripped symbol source for desktop heaptrack_interpret.
  cp -f "$INSTALL_DIR/bin/jellyfin-native" "$BUILD_DIR/jellyfin-native.unstripped" 2>/dev/null || true
else
  echo "Skipping heaptrack bundle (set BUNDLE_HEAPTRACK=1 after building heaptrack to enable)"
fi

echo "Stripping staged shared libraries"
while IFS= read -r -d '' library; do
  "$STRIP_BIN" --strip-unneeded "$library"
done < <(find "$STAGE_LIB" -type f -name '*.so*' -print0)
fi

if (( DO_PACKAGE )); then
[[ -f "$APP_DIR/appinfo.json" && -x "$STAGE_BIN/jellyfin-native" ]] || {
  echo "error: staged webOS app missing; run '$0 stage' first" >&2
  exit 1
}
rm -f "$BUILD_DIR"/*.ipk
# Plain ares-package output, no maintainer scripts. Hardware decode LS2 access
# comes from appinstalld's rolegen (/usr/share/rolegen/templates/NDK.*), which
# generates media-client roles for every native dev app from appinfo's "main"
# executable — the same mechanism Kodi relies on for non-rooted TVs.
npx -y -p @webos-tools/cli@3.2.3 ares-package "$APP_DIR" --outdir "$BUILD_DIR"
fi
