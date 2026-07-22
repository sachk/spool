#!/usr/bin/env bash
set -euo pipefail

APP_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
# shellcheck source=tools/lib/build-common.sh
source "$APP_ROOT/tools/lib/build-common.sh"
ensure_native_shell "$APP_ROOT" "$APP_ROOT/tools/package-appimage.sh" "$@"
# shellcheck source=tools/lib/qt-deploy.sh
source "$APP_ROOT/tools/lib/qt-deploy.sh"
# shellcheck source=tools/lib/manifest-sources.sh
source "$APP_ROOT/tools/lib/manifest-sources.sh"
TOOL_MANIFEST="${LINUXDEPLOY_MANIFEST:-$APP_ROOT/tools/manifests/linuxdeploy.json}"
FFMPEG_CAPABILITY_MANIFEST="${FFMPEG_CAPABILITY_MANIFEST:-$APP_ROOT/tools/manifests/ffmpeg-capabilities.json}"
APP_VERSION="$(read_project_version "$APP_ROOT")"
BUILD_ROOT="${BUILD_ROOT:-$APP_ROOT/build/linux-release/install/bin}"
MPV_PREFIX="${MPV_PREFIX:-$APP_ROOT/build/linux-release/mpv-prefix}"
APPDIR="${APPDIR:-$APP_ROOT/build/appimage/AppDir}"
ARTIFACT_DIR="${ARTIFACT_DIR:-$APP_ROOT/dist}"
LINUXDEPLOY="${LINUXDEPLOY:-$APP_ROOT/build/appimage/linuxdeploy-x86_64.AppImage}"
QT_PLUGIN="${QT_PLUGIN:-$APP_ROOT/build/appimage/linuxdeploy-plugin-qt-x86_64.AppImage}"
APPIMAGE_RUNTIME="${APPIMAGE_RUNTIME:-$APP_ROOT/build/appimage/runtime-x86_64}"

append_library_path() {
  local dir="$1"
  [[ -d "$dir" ]] || return 0
  case ":${LD_LIBRARY_PATH:-}:" in
    *":$dir:"*) ;;
    *) export LD_LIBRARY_PATH="$dir${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" ;;
  esac
}

prepend_path() {
  local dir="$1"
  [[ -d "$dir" ]] || return 0
  case ":$PATH:" in
    *":$dir:"*) ;;
    *) export PATH="$dir:$PATH" ;;
  esac
}

is_bundleable_elf_dep() {
  local dep="$1"
  local base="${dep##*/}"
  [[ -f "$dep" ]] || return 1
  case "$dep" in
    /nix/store/*-glibc-*) return 1 ;;
  esac
  case "$base" in
    ld-linux*.so*|libc.so*|libdl.so*|libm.so*|libpthread.so*|libresolv.so*|librt.so*|libutil.so*)
      return 1
      ;;
    libGLES*.so*|libglapi*.so*|libgbm*.so*)
      return 1
      ;;
  esac
  case "$dep" in
    /nix/store/*|"$MPV_PREFIX"/*) return 0 ;;
    *) return 1 ;;
  esac
}

copy_elf_deps() {
  local deferred_dep="${1:-}"
  local elf
  local dep
  local copied=1
  local roots=("$APPDIR/usr/bin" "$APPDIR/usr/lib")
  [[ -d "$APPDIR/usr/plugins" ]] && roots+=("$APPDIR/usr/plugins")
  [[ -d "$APPDIR/usr/qml" ]] && roots+=("$APPDIR/usr/qml")

  while (( copied )); do
    copied=0
    while IFS= read -r elf; do
      [[ -f "$elf" ]] || continue
      while IFS= read -r dep; do
        if [[ -n "$deferred_dep" && "${dep##*/}" == "$deferred_dep" ]]; then
          continue
        fi
        is_bundleable_elf_dep "$dep" || continue
        if [[ ! -e "$APPDIR/usr/lib/$(basename "$dep")" ]]; then
          cp -L "$dep" "$APPDIR/usr/lib/"
          chmod u+w "$APPDIR/usr/lib/$(basename "$dep")" 2>/dev/null || true
          copied=1
        fi
      done < <(ldd "$elf" 2>/dev/null | awk '/=> \// { print $3 } /^\// { print $1 }')
    done < <(find "${roots[@]}" -type f)
  done
}

copy_tree_if_exists() {
  local src="$1"
  local dst="$2"
  [[ -e "$src" ]] || return 0
  mkdir -p "$(dirname "$dst")"
  rm -rf "$dst"
  cp -a "$src" "$dst"
  chmod -R u+w "$dst" 2>/dev/null || true
}

qt_query() {
  local key="$1"
  if command -v qtpaths6 >/dev/null 2>&1; then
    qtpaths6 -query "$key" 2>/dev/null || true
  elif command -v qtpaths >/dev/null 2>&1; then
    qtpaths -query "$key" 2>/dev/null || true
  fi
}

bundle_wayland_plugins() {
  local plugins_dir
  plugins_dir="$(qt_query QT_INSTALL_PLUGINS)"
  [[ -d "$plugins_dir" ]] || return 0

  copy_tree_if_exists "$plugins_dir/platforms/libqwayland.so" \
    "$APPDIR/usr/plugins/platforms/libqwayland.so"
  copy_tree_if_exists "$plugins_dir/wayland-shell-integration" \
    "$APPDIR/usr/plugins/wayland-shell-integration"
  copy_tree_if_exists "$plugins_dir/wayland-graphics-integration-client" \
    "$APPDIR/usr/plugins/wayland-graphics-integration-client"
  copy_tree_if_exists "$plugins_dir/wayland-decoration-client" \
    "$APPDIR/usr/plugins/wayland-decoration-client"
}

prime_linuxdeploy_qt_plugins() {
  local root plugins_dir plugin_dir platform
  local plugin_roots=()

  plugins_dir="$(qt_query QT_INSTALL_PLUGINS)"
  [[ -d "$plugins_dir" ]] && plugin_roots+=("$plugins_dir")

  local qmake_roots=()
  IFS=: read -r -a qmake_roots <<< "${QMAKEPATH:-}"
  for root in "${qmake_roots[@]}"; do
    plugins_dir="$root/lib/qt-6/plugins"
    [[ -d "$plugins_dir" ]] || continue
    plugin_roots+=("$plugins_dir")
  done

  for plugins_dir in "${plugin_roots[@]}"; do
    for plugin_dir in \
      iconengines \
      imageformats \
      networkinformation \
      platforminputcontexts \
      sqldrivers \
      tls \
      wayland-decoration-client \
      wayland-graphics-integration-client \
      wayland-shell-integration; do
      copy_tree_if_exists "$plugins_dir/$plugin_dir" \
        "$APPDIR/usr/lib/qt-6/plugins/$plugin_dir"
    done

    mkdir -p "$APPDIR/usr/lib/qt-6/plugins/platforms"
    for platform in libqxcb.so libqwayland.so libqoffscreen.so; do
      [[ -f "$plugins_dir/platforms/$platform" ]] || continue
      cp -a "$plugins_dir/platforms/$platform" "$APPDIR/usr/lib/qt-6/plugins/platforms/"
      chmod u+w "$APPDIR/usr/lib/qt-6/plugins/platforms/$platform" 2>/dev/null || true
    done
  done
}

prune_appdir() {
  rm -rf \
    "$APPDIR/usr/qml/QtQuick/Controls/designer" \
    "$APPDIR/usr/qml/QtQuick/Controls/FluentWinUI3" \
    "$APPDIR/usr/qml/QtQuick/Controls/Fusion" \
    "$APPDIR/usr/lib/qt-6" \
    "$APPDIR/usr/qml/QtQuick/Controls/Imagine" \
    "$APPDIR/usr/qml/QtQuick/Controls/Material" \
    "$APPDIR/usr/qml/QtQuick/Controls/Universal" \
    "$APPDIR/usr/qml/QtQuick/Controls/FluentWinUI3.impl" \
    "$APPDIR/usr/qml/QtQuick/Dialogs/quickimpl/+Fusion" \
    "$APPDIR/usr/qml/QtQuick/Dialogs/quickimpl/+Imagine" \
    "$APPDIR/usr/qml/QtQuick/Dialogs/quickimpl/+Material" \
    "$APPDIR/usr/qml/QtQuick/Dialogs/quickimpl/+Universal" \
    "$APPDIR/usr/qml/QtQuick/Dialogs/quickimpl/+FluentWinUI3" \
    "$APPDIR/usr/qml/QtQuick/tooling" \
    "$APPDIR/usr/qml/QtQuick/Shapes/DesignHelpers" \
    "$APPDIR/usr/qml/QtQuick/VectorImage/Helpers"

  find "$APPDIR/usr/lib" -maxdepth 1 -type f \( \
    -name 'libQt6QuickControls2FluentWinUI3*.so*' -o \
    -name 'libQt6QuickControls2Fusion*.so*' -o \
    -name 'libQt6QuickControls2Imagine*.so*' -o \
    -name 'libQt6QuickControls2Material*.so*' -o \
    -name 'libQt6QuickControls2Universal*.so*' -o \
    -name 'libQt6QuickDialogs2QuickImpl.so*' -o \
    -name 'libQt6QuickShapesDesignHelpers.so*' -o \
    -name 'libQt6QuickVectorImageHelpers.so*' \
  \) -delete

  find "$APPDIR/usr/plugins/sqldrivers" -maxdepth 1 -type f ! -name 'libqsqlite.so' -delete 2>/dev/null || true
  find "$APPDIR/usr/plugins/platforms" -maxdepth 1 -type f \
    ! -name 'libqxcb.so' ! -name 'libqwayland.so' -delete 2>/dev/null || true
  find "$APPDIR/usr/lib" -maxdepth 1 -type f \( \
    -name 'libGLES*.so*' -o \
    -name 'libglapi*.so*' -o \
    -name 'libgbm*.so*' \
  \) -delete

  # Translations: keep only English (linuxdeploy-plugin-qt copies all locales).
  if [[ -d "$APPDIR/usr/translations" ]]; then
    find "$APPDIR/usr/translations" -maxdepth 1 -type f -name '*.qm' \
      ! -name 'qtbase_en*.qm' ! -name 'qt_en*.qm' -delete 2>/dev/null || true
  fi
}

# Fail packaging if a GPL/external codec library or another manifest-forbidden
# dependency leaked into the AppImage closure.
audit_ffmpeg_closure() {
  python3 "$APP_ROOT/tools/ffmpeg-capabilities.py" --manifest "$FFMPEG_CAPABILITY_MANIFEST" \
    audit-closure "$APPDIR/usr/lib" "$APPDIR/usr/bin"
}

patchelf_set_rpath() {
  local rpath="$1"
  local elf="$2"
  # Some linuxdeploy/AppImage runtime ELFs can make this host patchelf abort.
  # RPATH fixes are best-effort here and were already non-fatal; running
  # patchelf through a child Bash keeps that shell's signal diagnostic out of
  # otherwise successful package logs.
  bash -c 'patchelf --set-rpath "$1" "$2" >/dev/null 2>&1' _ "$rpath" "$elf" >/dev/null 2>&1 || true
}

set_appdir_rpaths() {
  local elf rel depth prefix
  while IFS= read -r elf; do
    file -b "$elf" | grep -q 'ELF' || continue
    case "$elf" in
      "$APPDIR/usr/bin/"*) patchelf_set_rpath '$ORIGIN/../lib' "$elf" ;;
      "$APPDIR/usr/lib/"*) patchelf_set_rpath '$ORIGIN' "$elf" ;;
      *)
        rel="${elf#"$APPDIR/usr/"}"
        depth=$(awk -F/ '{ print NF - 1 }' <<< "$rel")
        prefix='$ORIGIN'
        while (( depth > 0 )); do
          prefix="$prefix/.."
          depth=$((depth - 1))
        done
        patchelf_set_rpath "$prefix/lib:\$ORIGIN" "$elf"
        ;;
    esac
  done < <(find "$APPDIR/usr" -type f)
}

if [[ ! -x "$BUILD_ROOT/jellyfin-native" ]]; then
  echo "error: build output not found at $BUILD_ROOT/jellyfin-native" >&2
  exit 1
fi

mkdir -p "$ARTIFACT_DIR" "$(dirname "$LINUXDEPLOY")"
if [[ -d "$APPDIR" ]]; then
  chmod -R u+w "$APPDIR" 2>/dev/null || true
fi
rm -rf "$APPDIR"
mkdir -p "$APPDIR/usr/bin" "$APPDIR/usr/lib" "$APPDIR/usr/share/applications" \
  "$APPDIR/usr/share/icons/hicolor/256x256/apps" "$APPDIR/usr/share/jellyfin-native/notices"

cp -f "$BUILD_ROOT/jellyfin-native" "$APPDIR/usr/bin/jellyfin-native"
find "$MPV_PREFIX/lib" -name 'libmpv.so*' -exec cp -a {} "$APPDIR/usr/lib/" \;
if command -v secret-tool >/dev/null 2>&1; then
  cp -f "$(command -v secret-tool)" "$APPDIR/usr/bin/secret-tool"
fi
cp -f "$APP_ROOT/app/icon.png" "$APPDIR/usr/share/icons/hicolor/256x256/apps/jellyfin-native.png"
cp -f "$APP_ROOT/app/icon.png" "$APPDIR/jellyfin-native.png"
mkdir -p "$APPDIR/usr/share/metainfo"
cp -f "$APP_ROOT/app/com.sachk.tern.metainfo.xml" \
  "$APPDIR/usr/share/metainfo/jellyfin-native.appdata.xml"
cp -f "$APP_ROOT/app/notices/OPEN_SOURCE_NOTICES.txt" "$APP_ROOT/LICENSE" \
  "$APP_ROOT/qml/fonts/IBMPlexSans-LICENSE.txt" "$APP_ROOT/qml/fonts/MaterialIcons-LICENSE.txt" \
  "$APPDIR/usr/share/jellyfin-native/notices/"
cat > "$APPDIR/usr/share/jellyfin-native/fonts.conf" <<'FONTCONFIG'
<?xml version="1.0"?>
<!DOCTYPE fontconfig SYSTEM "urn:fontconfig:fonts.dtd">
<fontconfig>
  <dir>/usr/share/fonts</dir>
  <dir prefix="xdg">fonts</dir>
  <cachedir prefix="xdg">fontconfig</cachedir>
  <config></config>
</fontconfig>
FONTCONFIG
cat > "$APPDIR/AppRun" <<'APPRUN'
#!/usr/bin/env bash
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
export PATH="$HERE/usr/bin${PATH:+:$PATH}"
unset QML2_IMPORT_PATH QML_IMPORT_PATH QT_PLUGIN_PATH QT_QPA_PLATFORM_PLUGIN_PATH
export FONTCONFIG_FILE="$HERE/usr/share/jellyfin-native/fonts.conf"
if [[ -d /run/opengl-driver/lib ]]; then
  export LD_LIBRARY_PATH="$HERE/usr/lib:/run/opengl-driver/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
else
  export LD_LIBRARY_PATH="$HERE/usr/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
fi
if [[ -z "${__EGL_VENDOR_LIBRARY_DIRS:-}" && -d /run/opengl-driver/share/glvnd/egl_vendor.d ]]; then
  export __EGL_VENDOR_LIBRARY_DIRS=/run/opengl-driver/share/glvnd/egl_vendor.d
fi
if [[ -z "${__EGL_VENDOR_LIBRARY_FILENAMES:-}" && -z "${__EGL_VENDOR_LIBRARY_DIRS:-}" && -d /run/opengl-driver/lib ]]; then
  egl_vendor_dir="${XDG_RUNTIME_DIR:-/tmp}/jellyfin-native-egl-vendors"
  mkdir -p "$egl_vendor_dir"
  rm -f "$egl_vendor_dir"/*.json
  for egl_vendor in /run/opengl-driver/lib/libEGL_*.so*; do
    [[ -e "$egl_vendor" ]] || continue
    egl_name="${egl_vendor##*/}"
    egl_name="${egl_name#libEGL_}"
    egl_name="${egl_name%%.so*}"
    cat > "$egl_vendor_dir/10_${egl_name}.json" <<EOF
{
  "file_format_version": "1.0.0",
  "ICD": {
    "library_path": "$egl_vendor"
  }
}
EOF
  done
  if compgen -G "$egl_vendor_dir/*.json" >/dev/null; then
    export __EGL_VENDOR_LIBRARY_DIRS="$egl_vendor_dir"
  fi
fi
export QT_PLUGIN_PATH="$HERE/usr/plugins"
export QT_QPA_PLATFORM_PLUGIN_PATH="$HERE/usr/plugins/platforms"
export QML2_IMPORT_PATH="$HERE/usr/qml"
export QML_IMPORT_PATH="$HERE/usr/qml"
export QT_QPA_PLATFORM="${QT_QPA_PLATFORM:-wayland;xcb}"
exec "$HERE/usr/bin/jellyfin-native" "$@"
APPRUN
chmod +x "$APPDIR/AppRun"

cat > "$APPDIR/usr/share/applications/jellyfin-native.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=Jellyfin Native
Exec=jellyfin-native
Icon=jellyfin-native
Categories=AudioVideo;Video;
DESKTOP
cp -f "$APPDIR/usr/share/applications/jellyfin-native.desktop" "$APPDIR/jellyfin-native.desktop"

download_verified \
  "$(manifest_tool_field "$TOOL_MANIFEST" linuxdeploy url)" \
  "$(manifest_tool_field "$TOOL_MANIFEST" linuxdeploy sha256)" \
  "$LINUXDEPLOY"
chmod +x "$LINUXDEPLOY"
download_verified \
  "$(manifest_tool_field "$TOOL_MANIFEST" linuxdeploy-plugin-qt url)" \
  "$(manifest_tool_field "$TOOL_MANIFEST" linuxdeploy-plugin-qt sha256)" \
  "$QT_PLUGIN"
chmod +x "$QT_PLUGIN"
download_verified \
  "$(manifest_tool_field "$TOOL_MANIFEST" type2-runtime url)" \
  "$(manifest_tool_field "$TOOL_MANIFEST" type2-runtime sha256)" \
  "$APPIMAGE_RUNTIME"

QT_DEPLOY_SHADOW="$APP_ROOT/build/appimage/qt-host-tools"
qt_deploy_path="$(qt_deploy_linuxdeploy_qt_shadow \
  "${APP_BUILD_NINJA:-$APP_ROOT/build/linux-release/app/build.ninja}" "$QT_DEPLOY_SHADOW")"
if [[ -z "$qt_deploy_path" || ! -x "$qt_deploy_path/qmlimportscanner" ]]; then
  echo "error: qmlimportscanner is required for AppImage QML deployment" >&2
  exit 1
fi
export PATH="$qt_deploy_path${PATH:+:$PATH}"

append_library_path "$MPV_PREFIX/lib"
IFS=':' read -r -a cmake_library_dirs <<< "${CMAKE_LIBRARY_PATH:-}"
for lib_dir in "${cmake_library_dirs[@]}"; do
  append_library_path "$lib_dir"
done
while IFS= read -r lib_dir; do
  append_library_path "$lib_dir"
done < <(find "$MPV_PREFIX/lib" -name 'libmpv.so*' -exec dirname {} \; | sort -u)
while IFS= read -r dep; do
  case "$dep" in
    /nix/store/*-glibc-*) continue ;;
  esac
  append_library_path "$(dirname "$dep")"
  is_bundleable_elf_dep "$dep" || continue
done < <(ldd "$APPDIR/usr/bin/jellyfin-native" "$APPDIR"/usr/lib/libmpv.so* 2>/dev/null | awk '/=> \// { print $3 } /^\// { print $1 }' | sort -u)

export EXTRA_PLATFORM_PLUGINS="${EXTRA_PLATFORM_PLUGINS:-libqwayland.so}"
export QML_SOURCES_PATHS="${QML_SOURCES_PATHS:-$APP_ROOT/qml}"
export APPIMAGE_EXTRACT_AND_RUN="${APPIMAGE_EXTRACT_AND_RUN:-1}"
export OUTPUT="${OUTPUT:-Jellyfin-Native-${APP_VERSION}-x86_64.AppImage}"

copy_elf_deps libQt6WebSockets.so.6
set_appdir_rpaths

prime_linuxdeploy_qt_plugins
"$QT_PLUGIN" --appdir "$APPDIR"

unset EXTRA_PLATFORM_PLUGINS
bundle_wayland_plugins
prune_appdir
copy_elf_deps
set_appdir_rpaths
audit_ffmpeg_closure

APPIMAGETOOL="${APPIMAGETOOL:-}"
if [[ -z "$APPIMAGETOOL" ]]; then
  linuxdeploy_extract="$APP_ROOT/build/appimage/linuxdeploy-extracted"
  linuxdeploy_sha="$(manifest_tool_field "$TOOL_MANIFEST" linuxdeploy sha256)"
  linuxdeploy_marker="$linuxdeploy_extract/.linuxdeploy-sha256"
  if [[ ! -x "$linuxdeploy_extract/plugins/linuxdeploy-plugin-appimage/appimagetool-prefix/usr/bin/appimagetool" ]] \
      || [[ ! -f "$linuxdeploy_marker" ]] \
      || [[ "$(<"$linuxdeploy_marker")" != "$linuxdeploy_sha" ]]; then
    rm -rf "$linuxdeploy_extract" "$APP_ROOT/build/appimage/squashfs-root"
    (
      cd "$APP_ROOT/build/appimage"
      "$LINUXDEPLOY" --appimage-extract >/dev/null
      mv squashfs-root "$linuxdeploy_extract"
      printf '%s\n' "$linuxdeploy_sha" >"$linuxdeploy_marker"
    )
  fi
  APPIMAGETOOL="$linuxdeploy_extract/plugins/linuxdeploy-plugin-appimage/appimagetool-prefix/usr/bin/appimagetool"
fi

prepend_path "$(dirname "$APPIMAGETOOL")"
ARCH="${ARCH:-x86_64}" "$APPIMAGETOOL" \
  --runtime-file "$APPIMAGE_RUNTIME" \
  "$APPDIR" "$ARTIFACT_DIR/$OUTPUT"
printf '%s\n' "$ARTIFACT_DIR/$OUTPUT"
