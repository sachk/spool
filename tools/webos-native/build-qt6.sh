#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
SDK_ROOT="${WEBOS_SDK_ROOT:-$ROOT/build/webos-sdk/arm-webos-linux-gnueabi_sdk-buildroot}"
SYSROOT="$SDK_ROOT/arm-webos-linux-gnueabi/sysroot"
QT_VERSION="${QT_VERSION:-6.8.3}"
JOBS="${JOBS:-$(nproc)}"
PHASE="${1:-all}"

SRC_DIR="$ROOT/build/qt6-src"
QTBASE_TARBALL="$SRC_DIR/qtbase-everywhere-src-$QT_VERSION.tar.xz"
QTWAYLAND_TARBALL="$SRC_DIR/qtwayland-everywhere-src-$QT_VERSION.tar.xz"
QTBASE_SRC="$SRC_DIR/qtbase-everywhere-src-$QT_VERSION"
QTWAYLAND_SRC="$SRC_DIR/qtwayland-everywhere-src-$QT_VERSION"

HOST_BUILD="$ROOT/build/qt6-host"
HOST_QTWAYLAND_BUILD="$ROOT/build/qt6-host-qtwayland"
HOST_INSTALL="$ROOT/build/qt6-host-install"
TARGET_QTBASE_BUILD="$ROOT/build/qt6-target-qtbase"
TARGET_QTWAYLAND_BUILD="$ROOT/build/qt6-target-qtwayland"
TARGET_STAGING="$ROOT/build/qt6-target-install"
TARGET_PREFIX="${QT_TARGET_PREFIX:-/opt/qt6-webos}"
TOOLCHAIN_FILE="$ROOT/tools/webos-native/qt6-webos-toolchain.cmake"
PATCH_DIR="$ROOT/tools/webos-native/patches"
HOST_WAYLAND_SCANNER_DEFAULT="$(command -v wayland-scanner || true)"
TARGET_WAYLAND_SCANNER_DEFAULT="$SDK_ROOT/bin/wayland-scanner"

apply_patch_if_needed() {
  local src_dir="$1"
  local patch_file="$2"
  local probe_file="$3"
  local probe="$4"

  if grep -Fq "$probe" "$src_dir/$probe_file"; then
    return 0
  fi

  (
    cd "$src_dir"
    git apply "$patch_file"
  )
}

apply_local_patches() {
  apply_patch_if_needed \
    "$QTBASE_SRC" \
    "$PATCH_DIR/qtbase-6.8.3-webos-qstorageinfo-linux.patch" \
    "src/corelib/io/qstorageinfo_linux.cpp" \
    "QT_EINTR_LOOP(statvfsResult, statvfs64(path.constData(), &statvfs_buf));"
  apply_patch_if_needed \
    "$QTBASE_SRC" \
    "$PATCH_DIR/qtbase-6.8.3-webos-qelfparser.patch" \
    "src/corelib/plugin/qelfparser_p.cpp" \
    "#  define EM_AARCH64 183"
  apply_patch_if_needed \
    "$QTWAYLAND_SRC" \
    "$PATCH_DIR/qtwayland-6.8.3-webos-wayland-version.patch" \
    "src/CMakeLists.txt" \
    'qt_find_package(Wayland 1.11 PROVIDED_TARGETS ${wayland_libs})'
}

fetch_sources() {
  mkdir -p "$SRC_DIR"
  if [[ ! -f "$QTBASE_TARBALL" ]]; then
    curl -L --fail \
      -o "$QTBASE_TARBALL" \
      "https://download.qt.io/official_releases/qt/6.8/$QT_VERSION/submodules/qtbase-everywhere-src-$QT_VERSION.tar.xz"
  fi
  if [[ ! -f "$QTWAYLAND_TARBALL" ]]; then
    curl -L --fail \
      -o "$QTWAYLAND_TARBALL" \
      "https://download.qt.io/official_releases/qt/6.8/$QT_VERSION/submodules/qtwayland-everywhere-src-$QT_VERSION.tar.xz"
  fi
  [[ -d "$QTBASE_SRC" ]] || tar -C "$SRC_DIR" -xf "$QTBASE_TARBALL"
  [[ -d "$QTWAYLAND_SRC" ]] || tar -C "$SRC_DIR" -xf "$QTWAYLAND_TARBALL"
  apply_local_patches
}

configure_host_qtbase() {
  cmake -S "$QTBASE_SRC" -B "$HOST_BUILD" -GNinja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$HOST_INSTALL" \
    -DQT_BUILD_EXAMPLES=OFF \
    -DQT_BUILD_TESTS=OFF \
    -DINPUT_opengl=no \
    -DFEATURE_gui=ON \
    -DFEATURE_widgets=OFF \
    -DFEATURE_network=OFF \
    -DFEATURE_dbus=OFF \
    -DFEATURE_sql=OFF \
    -DFEATURE_xml=OFF \
    -DFEATURE_concurrent=OFF \
    -DFEATURE_xcb=OFF \
    -DFEATURE_gtk=OFF \
    -DFEATURE_accessibility=OFF
}

build_host_qtbase() {
  configure_host_qtbase
  cmake --build "$HOST_BUILD" --parallel "$JOBS"
  cmake --install "$HOST_BUILD"
}

configure_host_qtwayland() {
  local host_wayland_scanner="${HOST_WAYLAND_SCANNER:-$HOST_WAYLAND_SCANNER_DEFAULT}"
  if [[ -z "$host_wayland_scanner" ]]; then
    echo "error: host wayland-scanner not found in PATH; add it to shell.nix or set WAYLAND_SCANNER" >&2
    exit 1
  fi

  cmake -S "$QTWAYLAND_SRC" -B "$HOST_QTWAYLAND_BUILD" -GNinja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$HOST_INSTALL" \
    -DCMAKE_PREFIX_PATH="$HOST_INSTALL" \
    -DQT_BUILD_EXAMPLES=OFF \
    -DQT_BUILD_TESTS=OFF \
    -DWaylandScanner_EXECUTABLE="$host_wayland_scanner"
}

build_host_qtwayland() {
  configure_host_qtwayland
  cmake --build "$HOST_QTWAYLAND_BUILD" --parallel "$JOBS"
  cmake --install "$HOST_QTWAYLAND_BUILD"
}

configure_target_qtbase() {
  cmake -S "$QTBASE_SRC" -B "$TARGET_QTBASE_BUILD" -GNinja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN_FILE" \
    -DQT_HOST_PATH="$HOST_INSTALL" \
    -DQT_HOST_PATH_CMAKE_DIR="$HOST_INSTALL/lib/cmake" \
    -DCMAKE_STAGING_PREFIX="$TARGET_STAGING" \
    -DCMAKE_INSTALL_PREFIX="$TARGET_PREFIX" \
    -DQT_BUILD_EXAMPLES=OFF \
    -DQT_BUILD_TESTS=OFF \
    -DINPUT_opengl=es2 \
    -DFEATURE_egl=ON \
    -DFEATURE_gui=ON \
    -DFEATURE_widgets=ON \
    -DFEATURE_network=ON \
    -DFEATURE_dbus=OFF \
    -DFEATURE_sql=OFF \
    -DFEATURE_printsupport=OFF \
    -DFEATURE_xml=OFF \
    -DFEATURE_glib=OFF \
    -DFEATURE_icu=OFF \
    -DFEATURE_xcb=OFF \
    -DFEATURE_gtk=OFF \
    -DFEATURE_accessibility=OFF \
    -DFEATURE_xkbcommon=ON \
    -DINPUT_openssl=no
}

build_target_qtbase() {
  configure_target_qtbase
  cmake --build "$TARGET_QTBASE_BUILD" --parallel "$JOBS"
  cmake --install "$TARGET_QTBASE_BUILD"
}

configure_target_qtwayland() {
  local target_wayland_scanner="${TARGET_WAYLAND_SCANNER:-$TARGET_WAYLAND_SCANNER_DEFAULT}"
  if [[ ! -x "$target_wayland_scanner" ]]; then
    echo "error: target wayland-scanner not found at $target_wayland_scanner; set TARGET_WAYLAND_SCANNER" >&2
    exit 1
  fi

  cmake -S "$QTWAYLAND_SRC" -B "$TARGET_QTWAYLAND_BUILD" -GNinja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN_FILE" \
    -DQT_HOST_PATH="$HOST_INSTALL" \
    -DQT_HOST_PATH_CMAKE_DIR="$HOST_INSTALL/lib/cmake" \
    -DCMAKE_STAGING_PREFIX="$TARGET_STAGING" \
    -DCMAKE_INSTALL_PREFIX="$TARGET_PREFIX" \
    -DCMAKE_PREFIX_PATH="$TARGET_STAGING" \
    -DQT_BUILD_EXAMPLES=OFF \
    -DQT_BUILD_TESTS=OFF \
    -DWaylandScanner_EXECUTABLE="$target_wayland_scanner"
}

build_target_qtwayland() {
  configure_target_qtwayland
  cmake --build "$TARGET_QTWAYLAND_BUILD" --parallel "$JOBS"
  cmake --install "$TARGET_QTWAYLAND_BUILD"
}

show_summary() {
  echo
  echo "Host install:"
  find "$HOST_INSTALL/bin" -maxdepth 1 -type f | sed -n '1,40p'
  echo
  echo "Target platform plugins:"
  find "$TARGET_STAGING" -path '*/plugins/platforms/*' -type f | sed -n '1,80p'
}

case "$PHASE" in
  fetch)
    fetch_sources
    ;;
  host)
    fetch_sources
    build_host_qtbase
    ;;
  qtbase)
    fetch_sources
    [[ -d "$HOST_INSTALL" ]] || build_host_qtbase
    build_target_qtbase
    ;;
  qtwayland)
    fetch_sources
    [[ -f "$HOST_INSTALL/lib/cmake/Qt6Gui/Qt6GuiConfig.cmake" ]] || build_host_qtbase
    [[ -f "$HOST_INSTALL/lib/cmake/Qt6WaylandScannerTools/Qt6WaylandScannerToolsConfig.cmake" ]] || build_host_qtwayland
    [[ -d "$TARGET_STAGING" ]] || build_target_qtbase
    build_target_qtwayland
    ;;
  all)
    fetch_sources
    build_host_qtbase
    build_host_qtwayland
    build_target_qtbase
    build_target_qtwayland
    show_summary
    ;;
  *)
    echo "usage: $0 [fetch|host|qtbase|qtwayland|all]" >&2
    exit 1
    ;;
esac
