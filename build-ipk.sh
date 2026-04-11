#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WORKSPACE_ROOT="$(cd "$ROOT/.." && pwd)"
SDK_ROOT="${WEBOS_SDK_ROOT:-/tmp/webos-sdk/arm-webos-linux-gnueabi_sdk-buildroot}"
SYSROOT="$SDK_ROOT/arm-webos-linux-gnueabi/sysroot"
PREFIX="$SYSROOT/usr/local/webos-native"
MPV_BUILD="$WORKSPACE_ROOT/mpv/build/webos-libmpv"
QT6_HOST_PREFIX="$WORKSPACE_ROOT/build/qt6-611-host-install"
APP_DIR="$ROOT/app"
BUILD_DIR="$ROOT/build"
CMAKE_BUILD_DIR="$ROOT/build-arm"
INSTALL_DIR="$CMAKE_BUILD_DIR/install"
STAGE_LIB="$APP_DIR/lib"
STAGE_BIN="$APP_DIR/bin"
PATCHELF_BIN="$(command -v patchelf)"
STRIP_BIN="$SDK_ROOT/bin/arm-webos-linux-gnueabi-strip"
OPENAPI_GENERATOR_CLI="$(command -v openapi-generator-cli || true)"
OPENAPI_GENERATOR_CLI_JAR=""

if [[ -n "$OPENAPI_GENERATOR_CLI" ]]; then
  OPENAPI_GENERATOR_CLI_JAR="$(cd "$(dirname "$OPENAPI_GENERATOR_CLI")/.." && pwd)/share/java/openapi-generator-cli.jar"
fi

# Auto-detect static vs shared Qt: prefer static if available
QT6_STATIC_PREFIX="$WORKSPACE_ROOT/build/qt6-611-target-static-install"
QT6_SHARED_PREFIX="$WORKSPACE_ROOT/build/qt6-611-target-install"
QT_IS_STATIC=0
if [[ -f "$QT6_STATIC_PREFIX/lib/libQt6Core.a" ]]; then
  QT6_PREFIX="$QT6_STATIC_PREFIX"
  QT_IS_STATIC=1
  echo "Detected STATIC Qt6 build at $QT6_PREFIX"
elif [[ -d "$QT6_SHARED_PREFIX" ]]; then
  QT6_PREFIX="$QT6_SHARED_PREFIX"
  echo "Detected shared Qt6 build at $QT6_PREFIX"
else
  echo "error: no Qt6 install found" >&2
  exit 1
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

mkdir -p "$BUILD_DIR" "$STAGE_LIB" "$STAGE_BIN"
rm -rf "$INSTALL_DIR" "$APP_DIR/qt-plugins" "$APP_DIR/qt-qml"
rm -f "$STAGE_LIB"/* "$STAGE_BIN"/jellyfin-native "$STAGE_BIN"/qt.conf

if [[ "$QT_IS_STATIC" == "0" ]]; then
  mkdir -p "$APP_DIR/qt-plugins/platforms" \
           "$APP_DIR/qt-plugins/wayland-shell-integration" \
           "$APP_DIR/qt-plugins/wayland-graphics-integration-client" \
           "$APP_DIR/qt-plugins/imageformats" \
           "$APP_DIR/qt-plugins/sqldrivers"
fi
mkdir -p "$APP_DIR/qt-qml"

meson configure "$MPV_BUILD" -Dzlib=enabled
meson compile -C "$MPV_BUILD"

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
  -DCMAKE_TOOLCHAIN_FILE="$WORKSPACE_ROOT/tools/webos-native/qt6-webos-toolchain.cmake" \
  -DCMAKE_STAGING_PREFIX="$QT6_PREFIX" \
  -DCMAKE_FIND_ROOT_PATH="$SYSROOT;$QT6_PREFIX" \
  -DCMAKE_PREFIX_PATH="$QT6_PREFIX;$QT6_PREFIX/lib/cmake" \
  -DQt6_DIR="$QT6_PREFIX/lib/cmake/Qt6" \
  -DQt6OpenApiTools_DIR="$QT6_HOST_PREFIX/lib/cmake/Qt6OpenApiTools" \
  -DQT_HOST_PATH="$QT6_HOST_PREFIX" \
  -DOPENAPI_GENERATOR_CLI_JAR="$OPENAPI_GENERATOR_CLI_JAR" \
  -DCMAKE_INSTALL_PREFIX=/usr/palm/applications/com.sachk.tern \
  "${CMAKE_EXTRA_FLAGS[@]}"

cmake --build "$CMAKE_BUILD_DIR" --parallel
cmake --install "$CMAKE_BUILD_DIR" --prefix "$INSTALL_DIR"

cp -f "$INSTALL_DIR/bin/jellyfin-native" "$STAGE_BIN/jellyfin-native"

# mpv + ffmpeg shared libs are always needed (not statically linked)
cp -f "$MPV_BUILD/libmpv.so.2.5.0" "$STAGE_LIB/libmpv.so.2.5.0"
ln -sf libmpv.so.2.5.0 "$STAGE_LIB/libmpv.so.2"
ln -sf libmpv.so.2 "$STAGE_LIB/libmpv.so"

for lib in \
  "$PREFIX/lib/libavcodec.so.62" \
  "$PREFIX/lib/libavfilter.so.11" \
  "$PREFIX/lib/libavformat.so.62" \
  "$PREFIX/lib/libavutil.so.60" \
  "$PREFIX/lib/liblua5.2.so.0.0.0" \
  "$PREFIX/lib/libswresample.so.6" \
  "$PREFIX/lib/libswscale.so.9" \
  "$SYSROOT/usr/lib/libAcbAPI.so.1" \
  "$SYSROOT/usr/lib/libstdc++.so.6.0.33" \
  "$SYSROOT/lib/libgcc_s.so.1" \
  "$SYSROOT/usr/lib/libpcre2-16.so.0.13.0"
do
  cp -f "$lib" "$STAGE_LIB/"
done

cp -L "$SYSROOT/usr/lib/libjpeg.so.8.2.2" "$STAGE_LIB/"
cp -L "$SYSROOT/usr/lib/libpng16.so.16.46.0" "$STAGE_LIB/"

ln -sf libstdc++.so.6.0.33 "$STAGE_LIB/libstdc++.so.6"
ln -sf liblua5.2.so.0.0.0 "$STAGE_LIB/liblua5.2.so.0"
ln -sf liblua5.2.so.0 "$STAGE_LIB/liblua5.2.so"
ln -sf libpcre2-16.so.0.13.0 "$STAGE_LIB/libpcre2-16.so.0"
ln -sf libpcre2-16.so.0 "$STAGE_LIB/libpcre2-16.so"
ln -sf libjpeg.so.8.2.2 "$STAGE_LIB/libjpeg.so.8"
ln -sf libjpeg.so.8 "$STAGE_LIB/libjpeg.so"
ln -sf libpng16.so.16.46.0 "$STAGE_LIB/libpng16.so.16"
ln -sf libpng16.so.16 "$STAGE_LIB/libpng.so"

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
"$PATCHELF_BIN" --force-rpath --set-rpath '$ORIGIN' "$STAGE_LIB/libmpv.so.2.5.0"
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
