#!/usr/bin/env bash
set -euo pipefail

APP_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
# shellcheck source=tools/lib/build-common.sh
source "$APP_ROOT/tools/lib/build-common.sh"
ensure_native_shell "$APP_ROOT" "$APP_ROOT/tools/build-macos.sh" "$@"
# shellcheck source=tools/lib/qt-deploy.sh
source "$APP_ROOT/tools/lib/qt-deploy.sh"
MPV_SRC="${MPV_SRC:-$APP_ROOT/mpv}"
BUILD_ROOT="${BUILD_ROOT:-$APP_ROOT/build/macos}"
MPV_BUILD="${MPV_BUILD:-$BUILD_ROOT/mpv}"
MPV_PREFIX="${MPV_PREFIX:-$BUILD_ROOT/mpv-prefix}"
APP_BUILD="${APP_BUILD:-$BUILD_ROOT/app}"
APP_INSTALL="${APP_INSTALL:-$BUILD_ROOT/install}"
DEPLOY_APP="${DEPLOY_APP:-1}"
SPOOL_MACOS_CREDENTIAL_SERVICE="${SPOOL_MACOS_CREDENTIAL_SERVICE:-com.sachk.spool.dev}"

setup_native_ccache "$APP_ROOT"
mkdir -p "$MPV_PREFIX" "$APP_BUILD" "$APP_INSTALL"
MACOS_ICON="$BUILD_ROOT/jellyfin-native.icns"
ICONSET="$BUILD_ROOT/jellyfin-native.iconset"
rm -rf "$ICONSET"
mkdir -p "$ICONSET"
for size in 16 32 128 256 512; do
  cp -f "$APP_ROOT/app/icons/png/spool/${size}.png" "$ICONSET/icon_${size}x${size}.png"
  doubled=$((size * 2))
  cp -f "$APP_ROOT/app/icons/png/spool/${doubled}.png" "$ICONSET/icon_${size}x${size}@2x.png"
done
iconutil -c icns "$ICONSET" -o "$MACOS_ICON"
rm -rf "$ICONSET"

native_mpv_args "$MPV_PREFIX" release macos
MPV_SETUP_ARGS=(
  "${MPV_NATIVE_ARGS[@]}"
)

mpv_meson_build "$MPV_SRC" "$MPV_BUILD" "${MPV_SETUP_ARGS[@]}"
prune_stale_mpv_libraries "$MPV_PREFIX" "$MPV_BUILD"
append_colon_path PKG_CONFIG_PATH "$MPV_PREFIX/lib/pkgconfig"

rm -rf "$APP_INSTALL/jellyfin-native.app"
cmake_build_app "$APP_ROOT" "$APP_BUILD" \
  -DCMAKE_BUILD_TYPE=Release \
  -DJELLYFIN_NATIVE_WEBOS=OFF \
  -DJELLYFIN_MACOS_ICON="$MACOS_ICON" \
  -DSPOOL_MACOS_CREDENTIAL_SERVICE="$SPOOL_MACOS_CREDENTIAL_SERVICE" \
  -DCMAKE_PREFIX_PATH="$MPV_PREFIX${CMAKE_PREFIX_PATH:+;$CMAKE_PREFIX_PATH}" \
  -DCMAKE_INSTALL_PREFIX="$APP_INSTALL"

if [[ "$DEPLOY_APP" == "1" ]]; then
  command -v macdeployqt >/dev/null 2>&1 || {
    echo "error: DEPLOY_APP=1 requires macdeployqt" >&2
    exit 1
  }
  [[ -d "$APP_INSTALL/jellyfin-native.app" ]] || {
    echo "error: app bundle missing at $APP_INSTALL/jellyfin-native.app" >&2
    exit 1
  }


  macdeployqt_shadow="$(qt_deploy_macdeployqt_shadow \
    "$APP_BUILD/build.ninja" \
    "$BUILD_ROOT/qt-tools-shadow" \
    "$(command -v macdeployqt)")"
  "$macdeployqt_shadow" "$APP_INSTALL/jellyfin-native.app" -qmldir="$APP_ROOT/qml" -no-strip

  canonicalize_library_alias() {
    local obsolete_name="$1"
    local canonical_name="$2"
    local frameworks="$APP_INSTALL/jellyfin-native.app/Contents/Frameworks"
    local obsolete="$frameworks/$obsolete_name"
    local canonical="$frameworks/$canonical_name"
    local binary dependency
    [[ -f "$obsolete" ]] || return 0
    [[ -f "$canonical" ]] || {
      echo "error: cannot replace $obsolete_name without $canonical_name" >&2
      exit 1
    }
    while IFS= read -r binary; do
      file -b "$binary" | grep -q 'Mach-O' || continue
      while IFS= read -r dependency; do
        [[ "$(basename "$dependency")" == "$obsolete_name" ]] || continue
        chmod u+w "$binary"
        install_name_tool -change "$dependency" "@rpath/$canonical_name" "$binary"
      done < <(otool -L "$binary" 2>/dev/null | sed -n '2,$s/^[[:space:]]*\([^[:space:]]*\).*/\1/p')
    done < <(find "$APP_INSTALL/jellyfin-native.app" -type f)
    rm -f "$obsolete"
  }

  # nixpkgs Qt and ICU disagree over whether the ICU patch component belongs
  # in the Mach-O install name. macdeployqt otherwise materializes both names
  # as almost-identical signed libraries.
  canonicalize_library_alias libicudata.76.1.dylib libicudata.76.dylib
  canonicalize_library_alias libicuuc.76.1.dylib libicuuc.76.dylib

  rm -rf \
    "$APP_INSTALL/jellyfin-native.app/Contents/Resources/qml/QtQuick/Controls/FluentWinUI3" \
    "$APP_INSTALL/jellyfin-native.app/Contents/Resources/qml/QtQuick/Controls/Fusion" \
    "$APP_INSTALL/jellyfin-native.app/Contents/Resources/qml/QtQuick/Controls/Imagine" \
    "$APP_INSTALL/jellyfin-native.app/Contents/Resources/qml/QtQuick/Controls/Material" \
    "$APP_INSTALL/jellyfin-native.app/Contents/Resources/qml/QtQuick/Controls/Universal" \
    "$APP_INSTALL/jellyfin-native.app/Contents/Resources/qml/QtQuick/Controls/iOS" \
    "$APP_INSTALL/jellyfin-native.app/Contents/Resources/qml/QtQuick/Controls/Windows" \
    "$APP_INSTALL/jellyfin-native.app/Contents/Resources/qml/QtQuick/Dialogs/quickimpl/qml/+FluentWinUI3" \
    "$APP_INSTALL/jellyfin-native.app/Contents/Resources/qml/QtQuick/Dialogs/quickimpl/qml/+Fusion" \
    "$APP_INSTALL/jellyfin-native.app/Contents/Resources/qml/QtQuick/Dialogs/quickimpl/qml/+Imagine" \
    "$APP_INSTALL/jellyfin-native.app/Contents/Resources/qml/QtQuick/Dialogs/quickimpl/qml/+Material" \
    "$APP_INSTALL/jellyfin-native.app/Contents/Resources/qml/QtQuick/Dialogs/quickimpl/qml/+Universal"
  find "$APP_INSTALL/jellyfin-native.app/Contents/Frameworks" -maxdepth 1 \
    \( -name 'QtQuickControls2FluentWinUI3*' \
    -o -name 'QtQuickControls2Fusion*' \
    -o -name 'QtQuickControls2Imagine*' \
    -o -name 'QtQuickControls2IOS*' \
    -o -name 'QtQuickControls2Material*' \
    -o -name 'QtQuickControls2Universal*' \
    -o -name 'QtQuickControls2Windows*' \) \
    -exec rm -rf {} +
  find "$APP_INSTALL/jellyfin-native.app/Contents/PlugIns/quick" -maxdepth 1 -type f \
    \( -name '*fluentwinui3*' \
    -o -name '*fusionstyle*' \
    -o -name '*imaginestyle*' \
    -o -name '*iosstyle*' \
    -o -name '*materialstyle*' \
    -o -name '*universalstyle*' \
    -o -name '*windowsstyle*' \) \
    -delete
  dialogs_qmldir="$APP_INSTALL/jellyfin-native.app/Contents/Resources/qml/QtQuick/Dialogs/quickimpl/qmldir"
  if [[ -f "$dialogs_qmldir" ]]; then
    sed -i '' '\|qml/+\(Fusion\|Imagine\|Material\|Universal\|FluentWinUI3\)/|d' "$dialogs_qmldir"
  fi
  rm -f \
    "$APP_INSTALL/jellyfin-native.app/Contents/PlugIns/networkinformation/libqglib.dylib" \
    "$APP_INSTALL/jellyfin-native.app/Contents/PlugIns/sqldrivers/libqsqlodbc.dylib" \
    "$APP_INSTALL/jellyfin-native.app/Contents/PlugIns/sqldrivers/libqsqlpsql.dylib" \
    "$APP_INSTALL/jellyfin-native.app/Contents/Frameworks/libodbc.2.dylib" \
    "$APP_INSTALL/jellyfin-native.app/Contents/Frameworks/libpq.5.dylib"


  # macOS and GNU libiconv use the same install name but expose different
  # symbols (_iconv vs _libiconv). Keep macdeployqt's system-compatible copy
  # for libass, and give GNU libiconv a distinct name for libidn2.
  gnu_iconv="${GNU_ICONV_DYLIB:-}"
  if [[ ! -f "$gnu_iconv" ]] || ! /usr/bin/nm -gU "$gnu_iconv" | grep -q '_libiconv$'; then
    echo "error: GNU libiconv with the _libiconv ABI is unavailable: $gnu_iconv" >&2
    exit 1
  fi
  gnu_iconv_bundle="$APP_INSTALL/jellyfin-native.app/Contents/Frameworks/libiconv-gnu.2.dylib"
  rm -f "$gnu_iconv_bundle"
  cp -L "$gnu_iconv" "$gnu_iconv_bundle"
  if [[ -L "$gnu_iconv_bundle" || ! -f "$gnu_iconv_bundle" ]]; then
    echo "error: GNU libiconv was not materialized in the app bundle" >&2
    exit 1
  fi
  install_name_tool -id @rpath/libiconv-gnu.2.dylib "$gnu_iconv_bundle"
  while IFS= read -r binary; do
    if /usr/bin/nm -u "$binary" 2>/dev/null | grep -q '_libiconv$'; then
      while IFS= read -r dependency; do
        if [[ "$(basename "$dependency")" == "libiconv.2.dylib" ]]; then
          install_name_tool -change "$dependency" @rpath/libiconv-gnu.2.dylib "$binary"
        fi
      done < <(otool -L "$binary" 2>/dev/null | sed -n '2,$s/^[[:space:]]*\([^[:space:]]*\).*/\1/p')
    fi
  done < <(find "$APP_INSTALL/jellyfin-native.app" -type f)
  if ! otool -L "$APP_INSTALL/jellyfin-native.app/Contents/Frameworks/libidn2.0.dylib" \
    | grep -q 'libiconv-gnu\.2\.dylib'; then
    echo "error: libidn2 was not rebound to GNU libiconv" >&2
    exit 1
  fi
  # Artwork is served as webp, so a bundle without this reader shows an app with
  # no posters at all while every download still succeeds. macdeployqt reads one
  # plugin prefix, and qtimageformats is not the one qtbase reports.
  webp_plugin="$APP_INSTALL/jellyfin-native.app/Contents/PlugIns/imageformats/libqwebp.dylib"
  if [[ ! -f "$webp_plugin" ]]; then
    echo "error: qwebp imageformat plugin was not deployed to $webp_plugin" >&2
    echo "hint: SPOOL_QT_EXTRA_PLUGIN_DIRS must point at the qtimageformats plugin prefix (see flake.nix)" >&2
    exit 1
  fi
  mkdir -p "$APP_INSTALL/jellyfin-native.app/Contents/Resources/notices"
  cp -f "$APP_ROOT/app/notices/OPEN_SOURCE_NOTICES.txt" "$APP_ROOT/LICENSE" \
    "$APP_ROOT/qml/fonts/AtkinsonHyperlegible-LICENSE.txt" \
    "$APP_ROOT/qml/fonts/IBMPlexSans-LICENSE.txt" "$APP_ROOT/qml/fonts/PTRootUI-LICENSE.txt" \
    "$APP_ROOT/qml/fonts/MaterialIcons-LICENSE.txt" \
    "$APP_INSTALL/jellyfin-native.app/Contents/Resources/notices/"
  while IFS= read -r binary; do
    description="$(file -b "$binary")"
    [[ "$description" == *Mach-O* ]] || continue
    chmod u+w "$binary"
    if [[ "$description" == *"dynamically linked shared library"* ]]; then
      install_name_tool -id "@rpath/$(basename "$binary")" "$binary"
    fi
    while IFS= read -r rpath; do
      if [[ "$rpath" == /nix/store/* || "$rpath" == *"/build/"* ]]; then
        install_name_tool -delete_rpath "$rpath" "$binary"
      fi
    done < <(otool -l "$binary" | sed -n '/cmd LC_RPATH/{n;n;s/^[[:space:]]*path \([^[:space:]]*\).*/\1/p;}')
  done < <(find "$APP_INSTALL/jellyfin-native.app" -type f | sort)
  strip_bin="$(env -u DEVELOPER_DIR -u SDKROOT /usr/bin/xcrun --find strip)"
  find "$APP_INSTALL/jellyfin-native.app" -type f -name '*.qmltypes' -delete
  while IFS= read -r binary; do
    file -b "$binary" | grep -q 'Mach-O' || continue
    chmod u+w "$binary"
    "$strip_bin" -S -x "$binary"
  done < <(find "$APP_INSTALL/jellyfin-native.app" -type f | sort)
  python3 "$APP_ROOT/tools/package-audit.py" macho "$APP_INSTALL/jellyfin-native.app"
  python3 "$APP_ROOT/tools/package-audit.py" inventory "$APP_INSTALL/jellyfin-native.app" \
    --output "$BUILD_ROOT/jellyfin-native.inventory.tsv"
  python3 "$APP_ROOT/tools/ffmpeg-capabilities.py" \
    --manifest "$APP_ROOT/tools/manifests/ffmpeg-capabilities.json" \
    audit-closure "$APP_INSTALL/jellyfin-native.app"
fi

printf '%s\n' "$APP_INSTALL/jellyfin-native.app"
