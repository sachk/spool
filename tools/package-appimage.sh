#!/usr/bin/env bash
set -euo pipefail

APP_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_ROOT="${BUILD_ROOT:-$APP_ROOT/build/linux-release/install/bin}"
MPV_PREFIX="${MPV_PREFIX:-$APP_ROOT/build/linux-release/mpv-prefix}"
APPDIR="${APPDIR:-$APP_ROOT/build/appimage/AppDir}"
ARTIFACT_DIR="${ARTIFACT_DIR:-$APP_ROOT/dist}"
LINUXDEPLOY="${LINUXDEPLOY:-$APP_ROOT/build/appimage/linuxdeploy-x86_64.AppImage}"
QT_PLUGIN="${QT_PLUGIN:-$APP_ROOT/build/appimage/linuxdeploy-plugin-qt-x86_64.AppImage}"

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

copy_elf_deps() {
  local elf
  local dep
  local copied=1

  while (( copied )); do
    copied=0
    while IFS= read -r elf; do
      [[ -f "$elf" ]] || continue
      while IFS= read -r dep; do
        [[ -f "$dep" ]] || continue
        case "$dep" in
          /nix/store/*|"$MPV_PREFIX"/*) ;;
          *) continue ;;
        esac
        if [[ ! -e "$APPDIR/usr/lib/$(basename "$dep")" ]]; then
          cp -L "$dep" "$APPDIR/usr/lib/"
          chmod u+w "$APPDIR/usr/lib/$(basename "$dep")" 2>/dev/null || true
          copied=1
        fi
      done < <(ldd "$elf" 2>/dev/null | awk '/=> \// { print $3 } /^\// { print $1 }')
    done < <(find "$APPDIR/usr/bin" "$APPDIR/usr/lib" -type f)
  done
}

if [[ ! -x "$BUILD_ROOT/jellyfin-native" ]]; then
  echo "error: build output not found at $BUILD_ROOT/jellyfin-native" >&2
  exit 1
fi

mkdir -p "$ARTIFACT_DIR" "$(dirname "$LINUXDEPLOY")"
rm -rf "$APPDIR"
mkdir -p "$APPDIR/usr/bin" "$APPDIR/usr/lib" "$APPDIR/usr/share/applications" \
  "$APPDIR/usr/share/icons/hicolor/256x256/apps"

cp -f "$BUILD_ROOT/jellyfin-native" "$APPDIR/usr/bin/jellyfin-native"
find "$MPV_PREFIX/lib" -name 'libmpv.so*' -exec cp -a {} "$APPDIR/usr/lib/" \;
cp -f "$APP_ROOT/app/icon.png" "$APPDIR/usr/share/icons/hicolor/256x256/apps/jellyfin-native.png"
cp -f "$APP_ROOT/app/icon.png" "$APPDIR/jellyfin-native.png"
cat > "$APPDIR/AppRun" <<'APPRUN'
#!/usr/bin/env bash
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
export LD_LIBRARY_PATH="$HERE/usr/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
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

if [[ ! -x "$LINUXDEPLOY" ]]; then
  curl -L --fail -o "$LINUXDEPLOY" \
    https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage
  chmod +x "$LINUXDEPLOY"
fi
if [[ ! -x "$QT_PLUGIN" ]]; then
  curl -L --fail -o "$QT_PLUGIN" \
    https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage
  chmod +x "$QT_PLUGIN"
fi

append_library_path "$MPV_PREFIX/lib"
while IFS= read -r lib_dir; do
  append_library_path "$lib_dir"
done < <(find "$MPV_PREFIX/lib" -name 'libmpv.so*' -exec dirname {} \; | sort -u)
while IFS= read -r dep; do
  append_library_path "$(dirname "$dep")"
done < <(ldd "$APPDIR/usr/bin/jellyfin-native" "$APPDIR"/usr/lib/libmpv.so* 2>/dev/null | awk '/=> \// { print $3 } /^\// { print $1 }' | sort -u)
qmlscanner="$(command -v qmlimportscanner || true)"
if [[ -z "$qmlscanner" ]]; then
  qmlscanner_roots=()
  if command -v qtpaths6 >/dev/null 2>&1; then
    qmlscanner_roots+=("$(qtpaths6 -query QT_HOST_LIBEXECS 2>/dev/null || true)")
    qmlscanner_roots+=("$(qtpaths6 -query QT_INSTALL_LIBEXECS 2>/dev/null || true)")
  elif command -v qtpaths >/dev/null 2>&1; then
    qmlscanner_roots+=("$(qtpaths -query QT_HOST_LIBEXECS 2>/dev/null || true)")
    qmlscanner_roots+=("$(qtpaths -query QT_INSTALL_LIBEXECS 2>/dev/null || true)")
  fi
  IFS=':;' read -r -a cmake_prefix_roots <<< "${CMAKE_PREFIX_PATH:-}"
  qmlscanner_roots+=("${cmake_prefix_roots[@]}")
  for root in "${qmlscanner_roots[@]}"; do
    [[ -n "$qmlscanner" ]] && break
    [[ -n "$root" ]] || continue
    for candidate in "$root/qmlimportscanner" "$root/bin/qmlimportscanner" "$root/libexec/qmlimportscanner"; do
      if [[ -x "$candidate" ]]; then
        qmlscanner="$candidate"
        break
      fi
    done
  done
fi
if [[ -z "$qmlscanner" && -f "$APP_ROOT/build/linux-release/app/build.ninja" ]]; then
  qmlscanner="$(awk '/qmlimportscanner/ {
    for (i = 1; i <= NF; ++i) {
      if ($i ~ /\/qmlimportscanner$/) {
        print $i
        exit
      }
    }
  }' "$APP_ROOT/build/linux-release/app/build.ninja")"
fi
if [[ -n "$qmlscanner" ]]; then
  qtpaths6_bin="$(command -v qtpaths6 || true)"
  qtpaths_bin="$(command -v qtpaths || true)"
  qml_import_dir="$(cd "$(dirname "$qmlscanner")/.." && pwd)/lib/qt-6/qml"
  if [[ -d "$qml_import_dir" ]]; then
    qt_shadow="$APP_ROOT/build/appimage/qt-shadow/bin"
    rm -rf "$qt_shadow"
    mkdir -p "$qt_shadow"
    cat > "$qt_shadow/qmlimportscanner" <<EOF
#!/usr/bin/env bash
args=()
for arg in "\$@"; do
  if [[ "\$arg" == *-qtbase-*/lib/qt-6/qml && ! -d "\$arg" ]]; then
    args+=("$qml_import_dir")
  else
    args+=("\$arg")
  fi
done
exec "$qmlscanner" "\${args[@]}"
EOF
    chmod +x "$qt_shadow/qmlimportscanner"
    if [[ -n "$qtpaths6_bin" ]]; then
      cat > "$qt_shadow/qtpaths6" <<EOF
#!/usr/bin/env bash
if [[ "\$1" == "-query" && "\$2" == "QT_INSTALL_QML" ]]; then
  printf '%s\n' "$qml_import_dir"
else
  exec "$qtpaths6_bin" "\$@"
fi
EOF
      chmod +x "$qt_shadow/qtpaths6"
    fi
    if [[ -n "$qtpaths_bin" ]]; then
      cat > "$qt_shadow/qtpaths" <<EOF
#!/usr/bin/env bash
if [[ "\$1" == "-query" && "\$2" == "QT_INSTALL_QML" ]]; then
  printf '%s\n' "$qml_import_dir"
else
  exec "$qtpaths_bin" "\$@"
fi
EOF
      chmod +x "$qt_shadow/qtpaths"
    elif [[ -n "$qtpaths6_bin" ]]; then
      ln -sf qtpaths6 "$qt_shadow/qtpaths"
    fi
    prepend_path "$qt_shadow"
    export QML_IMPORT_PATH="$qml_import_dir${QML_IMPORT_PATH:+:$QML_IMPORT_PATH}"
    export QML2_IMPORT_PATH="$qml_import_dir${QML2_IMPORT_PATH:+:$QML2_IMPORT_PATH}"
  else
    prepend_path "$(dirname "$qmlscanner")"
  fi
fi
export EXTRA_QT_PLUGINS="${EXTRA_QT_PLUGINS:-wayland-shell-integration/libxdg-shell.so;wayland-graphics-integration-client/libqt-plugin-wayland-egl.so}"
export QML_SOURCES_PATHS="${QML_SOURCES_PATHS:-$APP_ROOT/qml}"
export APPIMAGE_EXTRACT_AND_RUN="${APPIMAGE_EXTRACT_AND_RUN:-1}"
export OUTPUT="${OUTPUT:-Jellyfin-Native-x86_64.AppImage}"

copy_elf_deps
patchelf --set-rpath '$ORIGIN/../lib' "$APPDIR/usr/bin/jellyfin-native"
while IFS= read -r lib; do
  file -b "$lib" | grep -q 'ELF' || continue
  patchelf --set-rpath '$ORIGIN' "$lib" 2>/dev/null || true
done < <(find "$APPDIR/usr/lib" -type f -name '*.so*')

"$QT_PLUGIN" --appdir "$APPDIR"

APPIMAGETOOL="${APPIMAGETOOL:-}"
if [[ -z "$APPIMAGETOOL" ]]; then
  linuxdeploy_extract="$APP_ROOT/build/appimage/linuxdeploy-extracted"
  if [[ ! -x "$linuxdeploy_extract/plugins/linuxdeploy-plugin-appimage/appimagetool-prefix/usr/bin/appimagetool" ]]; then
    rm -rf "$linuxdeploy_extract" "$APP_ROOT/build/appimage/squashfs-root"
    (
      cd "$APP_ROOT/build/appimage"
      "$LINUXDEPLOY" --appimage-extract >/dev/null
      mv squashfs-root "$linuxdeploy_extract"
    )
  fi
  APPIMAGETOOL="$linuxdeploy_extract/plugins/linuxdeploy-plugin-appimage/appimagetool-prefix/usr/bin/appimagetool"
fi

prepend_path "$(dirname "$APPIMAGETOOL")"
ARCH="${ARCH:-x86_64}" "$APPIMAGETOOL" "$APPDIR" "$ARTIFACT_DIR/$OUTPUT"
printf '%s\n' "$ARTIFACT_DIR/$OUTPUT"
