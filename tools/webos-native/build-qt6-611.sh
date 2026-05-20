#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
SDK_ROOT="${WEBOS_SDK_ROOT:-$ROOT/build/webos-sdk/arm-webos-linux-gnueabi_sdk-buildroot}"
SYSROOT="$SDK_ROOT/arm-webos-linux-gnueabi/sysroot"
QT_VERSION="${QT_VERSION:-6.11.0}"
JOBS="${JOBS:-$(nproc)}"
QT_STATIC="${QT_STATIC:-0}"
PHASE="${1:-all}"

SRC_DIR="$ROOT/build/qt6-src"
MAJOR_MINOR="${QT_VERSION%.*}"

QTBASE_TARBALL="$SRC_DIR/qtbase-everywhere-src-$QT_VERSION.tar.xz"
QTSHADERTOOLS_TARBALL="$SRC_DIR/qtshadertools-everywhere-src-$QT_VERSION.tar.xz"
QTDECLARATIVE_TARBALL="$SRC_DIR/qtdeclarative-everywhere-src-$QT_VERSION.tar.xz"
QTWAYLAND_TARBALL="$SRC_DIR/qtwayland-everywhere-src-$QT_VERSION.tar.xz"
QTOPENAPI_TARBALL="$SRC_DIR/qtopenapi-everywhere-src-$QT_VERSION.tar.xz"
QTIMAGEFORMATS_TARBALL="$SRC_DIR/qtimageformats-everywhere-src-$QT_VERSION.tar.xz"
QTVIRTUALKEYBOARD_TARBALL="$SRC_DIR/qtvirtualkeyboard-everywhere-src-$QT_VERSION.tar.xz"
QTSVG_TARBALL="$SRC_DIR/qtsvg-everywhere-src-$QT_VERSION.tar.xz"

QTBASE_SRC="$SRC_DIR/qtbase-everywhere-src-$QT_VERSION"
QTSHADERTOOLS_SRC="$SRC_DIR/qtshadertools-everywhere-src-$QT_VERSION"
QTDECLARATIVE_SRC="$SRC_DIR/qtdeclarative-everywhere-src-$QT_VERSION"
QTWAYLAND_SRC="$SRC_DIR/qtwayland-everywhere-src-$QT_VERSION"
QTOPENAPI_SRC="$SRC_DIR/qtopenapi-everywhere-src-$QT_VERSION"
QTIMAGEFORMATS_SRC="$SRC_DIR/qtimageformats-everywhere-src-$QT_VERSION"
QTVIRTUALKEYBOARD_SRC="$SRC_DIR/qtvirtualkeyboard-everywhere-src-$QT_VERSION"
# qtvirtualkeyboard depends on qtsvg for its default-style assets.
QTSVG_SRC="$SRC_DIR/qtsvg-everywhere-src-$QT_VERSION"

HOST_BUILD_ROOT="$ROOT/build/qt6-611-host"
HOST_INSTALL="$ROOT/build/qt6-611-host-install"

if [[ "$QT_STATIC" == "1" ]]; then
  TARGET_BUILD_ROOT="$ROOT/build/qt6-611-target-static"
  TARGET_STAGING="$ROOT/build/qt6-611-target-static-install"
  TARGET_PREFIX="${QT_TARGET_PREFIX:-/opt/qt6-webos-6.11-static}"
else
  TARGET_BUILD_ROOT="$ROOT/build/qt6-611-target"
  TARGET_STAGING="$ROOT/build/qt6-611-target-install"
  TARGET_PREFIX="${QT_TARGET_PREFIX:-/opt/qt6-webos-6.11}"
fi
TOOLCHAIN_FILE="$ROOT/tools/webos-native/qt6-webos-toolchain.cmake"
PATCH_DIR="$ROOT/tools/webos-native/patches"
HOST_WAYLAND_SCANNER_DEFAULT="$(command -v wayland-scanner || true)"
TARGET_WAYLAND_SCANNER_DEFAULT="$SDK_ROOT/bin/wayland-scanner"
OPENAPI_GENERATOR_CLI_JAR_DEFAULT=""

if [[ -n "$(command -v openapi-generator-cli || true)" ]]; then
  OPENAPI_GENERATOR_CLI_JAR_DEFAULT="$(dirname "$(dirname "$(readlink -f "$(command -v openapi-generator-cli)")")")/share/java/openapi-generator-cli.jar"
fi

download_submodule() {
  local module="$1"
  local tarball="$2"

  if [[ -f "$tarball" ]]; then
    return 0
  fi

  curl -L --fail \
    -o "$tarball" \
    "https://download.qt.io/official_releases/qt/$MAJOR_MINOR/$QT_VERSION/submodules/${module}-everywhere-src-$QT_VERSION.tar.xz"
}

extract_if_needed() {
  local tarball="$1"
  local src_dir="$2"

  [[ -d "$src_dir" ]] || tar -C "$SRC_DIR" -xf "$tarball"
}

apply_patch_if_needed() {
  local src_dir="$1"
  local patch_file="$2"
  # Legacy positional args ($3, $4) used to be a probe_file/probe pair
  # but trivial single-line probes routinely matched the *unpatched*
  # source and silently skipped. Detect application state by trying to
  # reverse-apply the patch instead — that's authoritative.
  shift 2

  if [[ ! -f "$patch_file" ]]; then
    echo "error: missing patch file $patch_file" >&2
    exit 1
  fi

  (
    cd "$src_dir"
    if patch -p1 --dry-run --reverse --silent < "$patch_file" >/dev/null 2>&1; then
      # Reverse-applies cleanly → already applied.
      return 0
    fi
    # Apply for real. Prefer plain patch(1) since tarball extractions
    # aren't git checkouts.
    patch -p1 < "$patch_file"
  )
}

apply_local_patches() {
  apply_patch_if_needed \
    "$QTBASE_SRC" \
    "$PATCH_DIR/qtbase-6.11.0-webos-qstorageinfo-linux.patch" \
    "src/corelib/io/qstorageinfo_linux.cpp" \
    "QT_EINTR_LOOP(statvfsResult, statvfs64(path.constData(), &statvfs_buf));"
  apply_patch_if_needed \
    "$QTBASE_SRC" \
    "$PATCH_DIR/qtbase-6.11.0-webos-qelfparser.patch" \
    "src/corelib/plugin/qelfparser_p.cpp" \
    "#  define EM_AARCH64 183"
  apply_patch_if_needed \
    "$QTBASE_SRC" \
    "$PATCH_DIR/qtbase-6.11.0-webos-wayland-no-opengl-forward-decl.patch"
  apply_patch_if_needed \
    "$QTWAYLAND_SRC" \
    "$PATCH_DIR/qtwayland-6.11.0-webos-wayland-version.patch" \
    "src/CMakeLists.txt" \
    'qt_find_package(Wayland 1.11 PROVIDED_TARGETS ${wayland_libs})'
  # In Qt 6.11 the qtwayland client code lives inside qtbase
  # (src/plugins/platforms/wayland/), not the qtwayland module — patch
  # it there so the QWaylandInputDevice::Pointer::updateCursor opt-out
  # we depend on for webOS magic remote cursor handling is built in.
  apply_patch_if_needed \
    "$QTBASE_SRC" \
    "$PATCH_DIR/qtbase-6.11.0-webos-no-cursor-set.patch" \
    "src/plugins/platforms/wayland/qwaylandinputdevice.cpp" \
    'JELLYFIN_QT_NO_CURSOR_SURFACE'
}

fetch_sources() {
  mkdir -p "$SRC_DIR"
  download_submodule qtbase "$QTBASE_TARBALL"
  download_submodule qtshadertools "$QTSHADERTOOLS_TARBALL"
  download_submodule qtdeclarative "$QTDECLARATIVE_TARBALL"
  download_submodule qtwayland "$QTWAYLAND_TARBALL"
  download_submodule qtopenapi "$QTOPENAPI_TARBALL"
  download_submodule qtimageformats "$QTIMAGEFORMATS_TARBALL"
  download_submodule qtsvg "$QTSVG_TARBALL"
  download_submodule qtvirtualkeyboard "$QTVIRTUALKEYBOARD_TARBALL"

  extract_if_needed "$QTBASE_TARBALL" "$QTBASE_SRC"
  extract_if_needed "$QTSHADERTOOLS_TARBALL" "$QTSHADERTOOLS_SRC"
  extract_if_needed "$QTDECLARATIVE_TARBALL" "$QTDECLARATIVE_SRC"
  extract_if_needed "$QTWAYLAND_TARBALL" "$QTWAYLAND_SRC"
  extract_if_needed "$QTOPENAPI_TARBALL" "$QTOPENAPI_SRC"
  extract_if_needed "$QTIMAGEFORMATS_TARBALL" "$QTIMAGEFORMATS_SRC"
  extract_if_needed "$QTSVG_TARBALL" "$QTSVG_SRC"
  extract_if_needed "$QTVIRTUALKEYBOARD_TARBALL" "$QTVIRTUALKEYBOARD_SRC"

  apply_local_patches
}

configure_host_qtbase() {
  cmake -S "$QTBASE_SRC" -B "$HOST_BUILD_ROOT/qtbase" -GNinja \
    --fresh \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$HOST_INSTALL" \
    -DQT_BUILD_EXAMPLES=OFF \
    -DQT_BUILD_TESTS=OFF \
    -DINPUT_opengl=no \
    -DFEATURE_gui=ON \
    -DFEATURE_network=ON \
    -DFEATURE_sql=ON \
    -DFEATURE_concurrent=ON \
    -DFEATURE_widgets=OFF \
    -DFEATURE_dbus=OFF \
    -DFEATURE_xml=OFF \
    -DFEATURE_xcb=OFF \
    -DFEATURE_gtk=OFF \
    -DFEATURE_accessibility=OFF
}

build_host_qtbase() {
  configure_host_qtbase
  cmake --build "$HOST_BUILD_ROOT/qtbase" --parallel "$JOBS"
  cmake --install "$HOST_BUILD_ROOT/qtbase"
}

configure_host_module() {
  local name="$1"
  local src="$2"
  local extra=("${@:3}")

  cmake -S "$src" -B "$HOST_BUILD_ROOT/$name" -GNinja \
    --fresh \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$HOST_INSTALL" \
    -DCMAKE_PREFIX_PATH="$HOST_INSTALL" \
    -DQT_BUILD_EXAMPLES=OFF \
    -DQT_BUILD_TESTS=OFF \
    "${extra[@]}"
}

build_host_module() {
  local name="$1"
  cmake --build "$HOST_BUILD_ROOT/$name" --parallel "$JOBS"
  cmake --install "$HOST_BUILD_ROOT/$name"
}

configure_target_qtbase() {
  local target_wayland_scanner="${TARGET_WAYLAND_SCANNER:-$TARGET_WAYLAND_SCANNER_DEFAULT}"
  if [[ ! -x "$target_wayland_scanner" ]]; then
    echo "error: target wayland-scanner not found at $target_wayland_scanner; set TARGET_WAYLAND_SCANNER" >&2
    exit 1
  fi

  local static_flags=()
  if [[ "$QT_STATIC" == "1" ]]; then
    static_flags=(
      -DBUILD_SHARED_LIBS=OFF
      -DCMAKE_BUILD_TYPE=MinSizeRel
      -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON
      -DCMAKE_C_FLAGS_MINSIZEREL="-Os -ffunction-sections -fdata-sections -DNDEBUG"
      -DCMAKE_CXX_FLAGS_MINSIZEREL="-Os -ffunction-sections -fdata-sections -DNDEBUG"
      -DCMAKE_EXE_LINKER_FLAGS="-Wl,--gc-sections"
      -DFEATURE_reduce_exports=ON
    )
    echo "Building Qt6 target STATIC with size optimizations"
  else
    static_flags=(
      -DCMAKE_BUILD_TYPE=Release
    )
  fi

  cmake -S "$QTBASE_SRC" -B "$TARGET_BUILD_ROOT/qtbase" -GNinja \
    --fresh \
    "${static_flags[@]}" \
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
    -DFEATURE_widgets=OFF \
    -DFEATURE_network=ON \
    -DFEATURE_sql=ON \
    -DFEATURE_concurrent=ON \
    -DFEATURE_dbus=OFF \
    -DFEATURE_printsupport=OFF \
    -DFEATURE_xml=OFF \
    -DFEATURE_glib=OFF \
    -DFEATURE_icu=OFF \
    -DFEATURE_xcb=OFF \
    -DFEATURE_gtk=OFF \
    -DFEATURE_accessibility=OFF \
    -DFEATURE_xkbcommon=ON \
    -DFEATURE_webp=ON \
    -DINPUT_openssl=no \
    -DWaylandScanner_EXECUTABLE="$target_wayland_scanner"
}

build_target_qtbase() {
  configure_target_qtbase
  cmake --build "$TARGET_BUILD_ROOT/qtbase" --parallel "$JOBS"
  cmake --install "$TARGET_BUILD_ROOT/qtbase"
}

configure_target_module() {
  local name="$1"
  local src="$2"
  local extra=("${@:3}")

  local build_type="Release"
  local module_flags=()
  if [[ "$QT_STATIC" == "1" ]]; then
    build_type="MinSizeRel"
    module_flags=(
      -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON
      -DCMAKE_C_FLAGS_MINSIZEREL="-Os -ffunction-sections -fdata-sections -DNDEBUG"
      -DCMAKE_CXX_FLAGS_MINSIZEREL="-Os -ffunction-sections -fdata-sections -DNDEBUG"
      -DCMAKE_EXE_LINKER_FLAGS="-Wl,--gc-sections"
    )
  fi

  cmake -S "$src" -B "$TARGET_BUILD_ROOT/$name" -GNinja \
    --fresh \
    -DCMAKE_BUILD_TYPE="$build_type" \
    "${module_flags[@]}" \
    -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN_FILE" \
    -DQT_HOST_PATH="$HOST_INSTALL" \
    -DQT_HOST_PATH_CMAKE_DIR="$HOST_INSTALL/lib/cmake" \
    -DCMAKE_STAGING_PREFIX="$TARGET_STAGING" \
    -DCMAKE_INSTALL_PREFIX="$TARGET_PREFIX" \
    -DCMAKE_PREFIX_PATH="$TARGET_STAGING" \
    -DQT_BUILD_EXAMPLES=OFF \
    -DQT_BUILD_TESTS=OFF \
    "${extra[@]}"
}

build_target_module() {
  local name="$1"
  cmake --build "$TARGET_BUILD_ROOT/$name" --parallel "$JOBS"
  cmake --install "$TARGET_BUILD_ROOT/$name"
}

build_all_host_modules() {
  local host_wayland_scanner="${HOST_WAYLAND_SCANNER:-$HOST_WAYLAND_SCANNER_DEFAULT}"
  local openapi_generator_cli_jar="${OPENAPI_GENERATOR_CLI_JAR:-$OPENAPI_GENERATOR_CLI_JAR_DEFAULT}"
  if [[ -z "$host_wayland_scanner" ]]; then
    echo "error: host wayland-scanner not found in PATH; add it to shell.nix or set HOST_WAYLAND_SCANNER" >&2
    exit 1
  fi
  if [[ ! -f "$openapi_generator_cli_jar" ]]; then
    echo "error: upstream OpenAPI generator jar not found at $openapi_generator_cli_jar; set OPENAPI_GENERATOR_CLI_JAR" >&2
    exit 1
  fi

  configure_host_module qtshadertools "$QTSHADERTOOLS_SRC"
  build_host_module qtshadertools

  configure_host_module qtdeclarative "$QTDECLARATIVE_SRC"
  build_host_module qtdeclarative

  configure_host_module qtopenapi "$QTOPENAPI_SRC" \
    -DOPENAPI_GENERATOR_CLI_JAR="$openapi_generator_cli_jar"
  build_host_module qtopenapi

  configure_host_module qtwayland "$QTWAYLAND_SRC" \
    -DWaylandScanner_EXECUTABLE="$host_wayland_scanner"
  build_host_module qtwayland

  # Host build of qtsvg + qtvirtualkeyboard so the keyboard module's
  # tooling/qmltyperegistrar can run during the target qmake/cmake step.
  configure_host_module qtsvg "$QTSVG_SRC"
  build_host_module qtsvg

  configure_host_module qtvirtualkeyboard "$QTVIRTUALKEYBOARD_SRC"
  build_host_module qtvirtualkeyboard
}

build_all_target_modules() {
  local target_wayland_scanner="${TARGET_WAYLAND_SCANNER:-$TARGET_WAYLAND_SCANNER_DEFAULT}"
  local openapi_generator_cli_jar="${OPENAPI_GENERATOR_CLI_JAR:-$OPENAPI_GENERATOR_CLI_JAR_DEFAULT}"
  if [[ ! -x "$target_wayland_scanner" ]]; then
    echo "error: target wayland-scanner not found at $target_wayland_scanner; set TARGET_WAYLAND_SCANNER" >&2
    exit 1
  fi
  if [[ ! -f "$openapi_generator_cli_jar" ]]; then
    echo "error: upstream OpenAPI generator jar not found at $openapi_generator_cli_jar; set OPENAPI_GENERATOR_CLI_JAR" >&2
    exit 1
  fi

  configure_target_module qtshadertools "$QTSHADERTOOLS_SRC"
  build_target_module qtshadertools

  configure_target_module qtdeclarative "$QTDECLARATIVE_SRC"
  build_target_module qtdeclarative

  configure_target_module qtopenapi "$QTOPENAPI_SRC" \
    -DOPENAPI_GENERATOR_CLI_JAR="$openapi_generator_cli_jar"
  build_target_module qtopenapi

  configure_target_module qtwayland "$QTWAYLAND_SRC" \
    -DWaylandScanner_EXECUTABLE="$target_wayland_scanner"
  build_target_module qtwayland

  configure_target_module qtimageformats "$QTIMAGEFORMATS_SRC"
  build_target_module qtimageformats

  configure_target_module qtsvg "$QTSVG_SRC"
  build_target_module qtsvg

  # The default keyboard style already moves the active key with the
  # Up/Down/Left/Right keys, so the magic remote D-pad works without
  # any extra configure flag.
  configure_target_module qtvirtualkeyboard "$QTVIRTUALKEYBOARD_SRC"
  build_target_module qtvirtualkeyboard
}

show_summary() {
  echo
  echo "Host install:"
  find "$HOST_INSTALL/lib/cmake" -maxdepth 1 -mindepth 1 -type d | sort | sed -n '1,80p'
  echo
  echo "Target install:"
  find "$TARGET_STAGING/lib/cmake" -maxdepth 1 -mindepth 1 -type d | sort | sed -n '1,120p'
}

case "$PHASE" in
  fetch)
    fetch_sources
    ;;
  host)
    fetch_sources
    build_host_qtbase
    build_all_host_modules
    ;;
  target)
    fetch_sources
    [[ -f "$HOST_INSTALL/lib/cmake/Qt6/Qt6Config.cmake" ]] || {
      build_host_qtbase
      build_all_host_modules
    }
    build_target_qtbase
    build_all_target_modules
    ;;
  all)
    fetch_sources
    build_host_qtbase
    build_all_host_modules
    build_target_qtbase
    build_all_target_modules
    show_summary
    ;;
  *)
    echo "usage: $0 [fetch|host|target|all]" >&2
    exit 1
    ;;
esac
