#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
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
MPV_SRC="${MPV_SRC:-$ROOT/mpv_webos}"
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
WEBOS_BUILD_MEMORY_PER_JOB_MIB="${WEBOS_BUILD_MEMORY_PER_JOB_MIB:-1536}"
WEBOS_BUILD_MEMORY_RESERVE_MIB="${WEBOS_BUILD_MEMORY_RESERVE_MIB:-2048}"
WEBOS_BUILD_JOBS="$(recommended_parallel_jobs "$WEBOS_BUILD_MEMORY_PER_JOB_MIB" "$WEBOS_BUILD_MEMORY_RESERVE_MIB")"

# Auto-detect static vs shared Qt: prefer static if available
QT6_STATIC_PREFIX="$ROOT/build/qt6-611-target-static-install"
QT6_SHARED_PREFIX="$ROOT/build/qt6-611-target-install"
QT_IS_STATIC=0
if (( DO_BUILD || DO_STAGE )); then
  ensure_webos_sdk_host_tools "$SDK_ROOT"

  if [[ -f "$QT6_STATIC_PREFIX/lib/libQt6Core.a" ]]; then
    QT6_PREFIX="$QT6_STATIC_PREFIX"
    QT_IS_STATIC=1
    echo "Detected STATIC Qt6 build at $QT6_PREFIX"
  elif [[ -d "$QT6_SHARED_PREFIX" ]]; then
    QT6_PREFIX="$QT6_SHARED_PREFIX"
    echo "Detected shared Qt6 build at $QT6_PREFIX"
  else
    echo "error: no Qt6 install found under $ROOT/build" >&2
    exit 1
  fi

  # Reject stale cached Qt prefixes that predate required local patches.
  QT_PATCH_MARKERS=(JELLYFIN_QT_NO_CURSOR_SURFACE)
  for marker in "${QT_PATCH_MARKERS[@]}"; do
    if ! grep -rqal "$marker" "$QT6_PREFIX/lib" 2>/dev/null; then
      echo "error: Qt at $QT6_PREFIX is missing patch marker '$marker'." >&2
      echo "       Rerun: bash $WEBOS_TOOLS_ROOT/build-qt6-611.sh" >&2
      exit 1
    fi
  done

  QT_GUI_CONFIG="$QT6_PREFIX/include/QtGui/qtgui-config.h"
  if ! grep -q '^#define QT_FEATURE_accessibility 1$' "$QT_GUI_CONFIG" 2>/dev/null; then
    echo "error: Qt at $QT6_PREFIX was built without accessibility support." >&2
    echo "       Rerun: bash $WEBOS_TOOLS_ROOT/build-qt6-611.sh" >&2
    exit 1
  fi
fi

copy_first_match() {
  local pattern="$1"
  local dest="$2"
  local source

  source="$(compgen -G "$pattern" | head -n 1 || true)"
  if [[ -z "$source" ]]; then
    echo "error: no file matched pattern $pattern" >&2
    exit 1
  fi
  cp -f "$source" "$dest"
}

copy_first_match_optional() {
  local pattern="$1"
  local dest="$2"
  local source

  source="$(compgen -G "$pattern" | head -n 1 || true)"
  if [[ -z "$source" ]]; then
    return 0
  fi
  cp -f "$source" "$dest"
}

copy_qml_module() {
  local module="$1"
  local source="$QT6_PREFIX/qml/$module"
  local target="$APP_DIR/qt-qml/$module"

  [[ -d "$source" ]] || return 0
  mkdir -p "$(dirname "$target")"
  rm -rf "$target"
  cp -a "$source" "$target"
}

if (( DO_BUILD )); then
describe_parallel_jobs "$WEBOS_BUILD_JOBS" "webOS app" "$WEBOS_BUILD_MEMORY_PER_JOB_MIB" "$WEBOS_BUILD_MEMORY_RESERVE_MIB"

DOVI_TOOL_ROOT="${DOVI_TOOL_ROOT:-$ROOT/build/third_party/dovi_tool}"
if [[ ! -f "$DOVI_TOOL_ROOT/dolby_vision/Cargo.toml" ]]; then
  echo "error: verified dovi_tool source not found at $DOVI_TOOL_ROOT" >&2
  echo "       Run: bash tools/webos-native/build-third-party.sh fetch" >&2
  exit 1
fi

echo "Building libdovi..."
(
  cd "$DOVI_TOOL_ROOT/dolby_vision"
  rustup target add arm-unknown-linux-gnueabi || true
  export CARGO_TARGET_ARM_UNKNOWN_LINUX_GNUEABI_LINKER="$SDK_ROOT/bin/arm-webos-linux-gnueabi-gcc"
  export CC_arm_unknown_linux_gnueabi="$SDK_ROOT/bin/arm-webos-linux-gnueabi-gcc"
  export CXX_arm_unknown_linux_gnueabi="$SDK_ROOT/bin/arm-webos-linux-gnueabi-g++"
  export AR_arm_unknown_linux_gnueabi="$SDK_ROOT/bin/arm-webos-linux-gnueabi-ar"
  export CARGO_BUILD_JOBS="${CARGO_BUILD_JOBS:-$WEBOS_BUILD_JOBS}"
  
  CARGO_BIN="cargo"
  if command -v rustup >/dev/null 2>&1; then
    CARGO_BIN="rustup run nightly cargo"
  fi
  $CARGO_BIN build --release --features capi,serde --target arm-unknown-linux-gnueabi --jobs "$WEBOS_BUILD_JOBS"
)
DOVI_LIB="$DOVI_TOOL_ROOT/dolby_vision/target/arm-unknown-linux-gnueabi/release/libdovi.a"
DOVI_INC="$DOVI_TOOL_ROOT/dolby_vision/include"

# Emit unwind tables + keep frame pointers so heaptrack's (crash-safe, non-
# libunwind) backtracer can produce deep call stacks for memory profiling.
# libunwind segfaults on this target, so .ARM.exidx/.eh_frame is the only path
# to per-function heap attribution. Negligible size/perf cost.
HEAPTRACK_UNWIND_FLAGS="${HEAPTRACK_UNWIND_FLAGS:--fasynchronous-unwind-tables -funwind-tables -fno-omit-frame-pointer -g}"
export CFLAGS="${CFLAGS:-} -I$DOVI_INC $HEAPTRACK_UNWIND_FLAGS"
export CXXFLAGS="${CXXFLAGS:-} -I$DOVI_INC $HEAPTRACK_UNWIND_FLAGS"
export LDFLAGS="${LDFLAGS:-} -L$(dirname "$DOVI_LIB")"
export PKG_CONFIG_PATH="$PREFIX/lib/pkgconfig"
export PKG_CONFIG_LIBDIR="$PREFIX/lib/pkgconfig:$SYSROOT/usr/lib/pkgconfig:$SYSROOT/usr/share/pkgconfig"
export PKG_CONFIG_SYSROOT_DIR="$SYSROOT"
# Host desktop sessions commonly export QML import paths for the system Qt/KDE
# stack. The webOS build must scan only the cross-built Qt prefix supplied below;
# otherwise qmlimportscanner can wander into Qt source/test fixtures and print
# parser errors for intentionally invalid QML files.
unset QML_IMPORT_PATH QML2_IMPORT_PATH NIXPKGS_QT6_QML_IMPORT_PATH

MPV_SETUP_ARGS=(
  --cross-file "$WEBOS_CROSS_FILE"
  --prefix /usr/local
  --libdir lib
  --buildtype release
  -Dcplayer=false
  -Dlibmpv=true
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
  "-Dc_link_args=-L$(dirname "$DOVI_LIB") -ldovi"
  "-Dcpp_link_args=-L$(dirname "$DOVI_LIB") -ldovi"
  # Explicit c_args/cpp_args: meson --reconfigure does not re-read CFLAGS env,
  # so pass the unwind flags here too (deep heaptrack stacks need them in libmpv).
  "-Dc_args=$HEAPTRACK_UNWIND_FLAGS"
  "-Dcpp_args=$HEAPTRACK_UNWIND_FLAGS"
)

if [[ -f "$MPV_BUILD/build.ninja" ]]; then
  meson setup --reconfigure "$MPV_BUILD" "$MPV_SRC" "${MPV_SETUP_ARGS[@]}"
else
  meson setup "$MPV_BUILD" "$MPV_SRC" "${MPV_SETUP_ARGS[@]}"
fi
meson compile -C "$MPV_BUILD" -j "$WEBOS_BUILD_JOBS"

# CMAKE flags: pass BUILD_SHARED_LIBS=OFF when linking against static Qt
CMAKE_EXTRA_FLAGS=()
if [[ "$QT_IS_STATIC" == "1" ]]; then
  CMAKE_EXTRA_FLAGS+=(
    -DBUILD_SHARED_LIBS=OFF
    -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON
  )
fi

cmake -S "$ROOT" -B "$CMAKE_BUILD_DIR" -GNinja \
  --fresh \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE="$WEBOS_TOOLS_ROOT/qt6-webos-toolchain.cmake" \
  -DCMAKE_STAGING_PREFIX="$QT6_PREFIX" \
  -DCMAKE_FIND_ROOT_PATH="$SYSROOT;$QT6_PREFIX" \
  -DCMAKE_PREFIX_PATH="$QT6_PREFIX;$QT6_PREFIX/lib/cmake" \
  -DQt6_DIR="$QT6_PREFIX/lib/cmake/Qt6" \
  -DQT_HOST_PATH="$QT6_HOST_PREFIX" \
  -DCMAKE_INSTALL_PREFIX=/usr/palm/applications/com.sachk.tern \
  "${CMAKE_EXTRA_FLAGS[@]}"

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
PATCHELF_BIN="$(command -v patchelf)"

if [[ "$QT_IS_STATIC" == "0" ]]; then
  mkdir -p "$APP_DIR/qt-plugins/platforms" \
           "$APP_DIR/qt-plugins/wayland-shell-integration" \
           "$APP_DIR/qt-plugins/wayland-graphics-integration-client" \
           "$APP_DIR/qt-plugins/imageformats" \
           "$APP_DIR/qt-plugins/sqldrivers"
fi

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

if [[ "$QT_IS_STATIC" == "0" ]]; then
  # --- Shared Qt: copy Qt shared libs, plugins, patchelf ---

  while IFS= read -r -d '' qtlib; do
    cp -f "$qtlib" "$STAGE_LIB/"
  done < <(find "$QT6_PREFIX/lib" -maxdepth 1 -type f -name 'libQt6*.so.*' -print0)

  for lib in "$STAGE_LIB"/libQt6*.so.*; do
    base="$(basename "$lib")"
    major="${base%%.so.*}.so.6"
    short="${base%%.so.*}.so"
    ln -sf "$base" "$STAGE_LIB/$major"
    ln -sf "$major" "$STAGE_LIB/$short"
  done

  copy_first_match "$QT6_PREFIX/plugins/platforms/libqwayland.so" "$APP_DIR/qt-plugins/platforms/"
  cp -f "$QT6_PREFIX/plugins/wayland-shell-integration/libwl-shell-plugin.so" "$APP_DIR/qt-plugins/wayland-shell-integration/"
  cp -f "$QT6_PREFIX/plugins/wayland-shell-integration/libqt-shell.so" "$APP_DIR/qt-plugins/wayland-shell-integration/"
  cp -f "$QT6_PREFIX/plugins/wayland-shell-integration/libivi-shell.so" "$APP_DIR/qt-plugins/wayland-shell-integration/"
  cp -f "$QT6_PREFIX/plugins/wayland-shell-integration/libxdg-shell.so" "$APP_DIR/qt-plugins/wayland-shell-integration/"
  cp -f "$QT6_PREFIX/plugins/wayland-graphics-integration-client/libqt-plugin-wayland-egl.so" \
    "$APP_DIR/qt-plugins/wayland-graphics-integration-client/"
  cp -f "$QT6_PREFIX/plugins/imageformats/libqjpeg.so" "$APP_DIR/qt-plugins/imageformats/"
  cp -f "$QT6_PREFIX/plugins/imageformats/libqgif.so" "$APP_DIR/qt-plugins/imageformats/"
  copy_first_match_optional "$QT6_PREFIX/plugins/imageformats/libqwebp.so" "$APP_DIR/qt-plugins/imageformats/"
  copy_first_match "$QT6_PREFIX/plugins/sqldrivers/libqsqlite.so" "$APP_DIR/qt-plugins/sqldrivers/"

  for lib in "$STAGE_LIB"/libQt6*.so.*; do
    "$PATCHELF_BIN" --force-rpath --set-rpath '$ORIGIN' "$lib"
  done
  for plugin in \
    "$APP_DIR/qt-plugins/platforms/libqwayland.so" \
    "$APP_DIR/qt-plugins/wayland-shell-integration/libwl-shell-plugin.so" \
    "$APP_DIR/qt-plugins/wayland-shell-integration/libqt-shell.so" \
    "$APP_DIR/qt-plugins/wayland-shell-integration/libivi-shell.so" \
    "$APP_DIR/qt-plugins/wayland-shell-integration/libxdg-shell.so" \
    "$APP_DIR/qt-plugins/wayland-graphics-integration-client/libqt-plugin-wayland-egl.so" \
    "$APP_DIR/qt-plugins/imageformats/libqjpeg.so" \
    "$APP_DIR/qt-plugins/imageformats/libqgif.so" \
    "$APP_DIR/qt-plugins/imageformats/libqwebp.so" \
    "$APP_DIR/qt-plugins/sqldrivers/libqsqlite.so"
  do
    [[ -f "$plugin" ]] || continue
    "$PATCHELF_BIN" --force-rpath --set-rpath '$ORIGIN/../../lib' "$plugin"
  done

  cat > "$STAGE_BIN/qt.conf" <<'QTCONF'
[Paths]
Plugins = ../qt-plugins
QmlImports = ../qt-qml
QTCONF
fi

# QML files are needed at runtime for both static and shared Qt
if [[ "$QT_IS_STATIC" == "0" ]]; then
  copy_qml_module "QtQml"
  copy_qml_module "QtQuick"
  copy_qml_module "QtQuick/Templates"
  copy_qml_module "QtQuick/Controls"
  copy_qml_module "QtQuick/Controls/impl"
  copy_qml_module "QtQuick/Controls/Basic"
  copy_qml_module "QtQuick/Controls/Basic/impl"
  copy_qml_module "QtQuick/Layouts"
  copy_qml_module "QtQml/Models"
  copy_qml_module "QtQml/WorkerScript"
  copy_qml_module "QtQuick/Window"
fi

# patchelf on mpv (always needed)
"$PATCHELF_BIN" --force-rpath --set-rpath '$ORIGIN' "$MPV_STAGED_LIBRARY"
"$PATCHELF_BIN" --force-rpath --set-rpath '$ORIGIN/../lib' "$STAGE_BIN/jellyfin-native"

# strip binary to reduce size
"$STRIP_BIN" --strip-unneeded "$STAGE_BIN/jellyfin-native"

if [[ "$QT_IS_STATIC" == "1" ]]; then
  # For static Qt, write a qt.conf pointing to QML files only
  cat > "$STAGE_BIN/qt.conf" <<'QTCONF'
[Paths]
QmlImports = ../qt-qml
QTCONF
fi

# --- Optional: bundle heaptrack memory recorder + launch shim --------------
# Controlled by BUNDLE_HEAPTRACK (default: auto -- bundle iff the cross-built
# recorder exists, built via tools/webos-native/build-heaptrack.sh). The shim
# replaces the binary at main=bin/jellyfin-native but is a transparent
# passthrough unless a runtime marker file is present, so a normal launch is
# unaffected and profiling can never break playback.
HEAPTRACK_INSTALL="${HEAPTRACK_INSTALL:-$ROOT/build/webos-heaptrack/install}"
HEAPTRACK_SHIM="$ROOT/tools/webos/heaptrack-launch-shim.sh"
if [[ "${BUNDLE_HEAPTRACK:-auto}" != "0" && -f "$HEAPTRACK_INSTALL/lib/heaptrack/libheaptrack_preload.so" ]]; then
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
  echo "Skipping heaptrack bundle (run tools/webos-native/build-heaptrack.sh to enable)"
fi
fi

if (( DO_PACKAGE )); then
[[ -f "$APP_DIR/appinfo.json" && -x "$STAGE_BIN/jellyfin-native" ]] || {
  echo "error: staged webOS app missing; run '$0 stage' first" >&2
  exit 1
}
rm -f "$BUILD_DIR"/*.ipk
npx -y -p @webos-tools/cli@3.2.3 ares-package "$APP_DIR" --outdir "$BUILD_DIR"

PACKAGING_DIR="$ROOT/packaging"
if [ -f "$PACKAGING_DIR/postinst" ] || [ -f "$PACKAGING_DIR/prerm" ]; then
  IPK="$(ls -1t "$BUILD_DIR"/*.ipk | head -n 1)"
  REPACK_DIR="$(mktemp -d)"
  (
    cd "$REPACK_DIR"
    ar x "$IPK"
    mkdir -p control_dir
    tar xzf control.tar.gz -C control_dir
    for script in postinst prerm; do
      if [ -f "$PACKAGING_DIR/$script" ]; then
        cp "$PACKAGING_DIR/$script" "control_dir/$script"
        chmod 755 "control_dir/$script"
      fi
    done
    (cd control_dir && tar czf ../control.tar.gz ./)
    ar rc "$IPK" debian-binary control.tar.gz data.tar.gz
  )
  rm -rf "$REPACK_DIR"
fi
fi
