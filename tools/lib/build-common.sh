#!/usr/bin/env bash
# Shared helpers for the native (non-webOS) build and packaging scripts:
# tools/build-linux-release.sh, tools/build-macos.sh, tools/package-appimage.sh.
#
# Source this file ("source tools/lib/build-common.sh"); do not execute it.
# It defines functions only and changes no global state.

append_colon_path() {
  local variable="$1"
  local dir="$2"
  local current="${!variable:-}"
  [[ -d "$dir" ]] || return 0
  case ":$current:" in
    *":$dir:"*) ;;
    *) printf -v "$variable" '%s%s%s' "$dir" "${current:+:}" "$current" ;;
  esac
  export "$variable"
}

setup_native_ccache() {
  local root="$1"
  export CCACHE_DIR="${CCACHE_DIR:-$root/.ccache}"
  export CCACHE_BASEDIR="${CCACHE_BASEDIR:-$root}"
  export CCACHE_COMPRESS="${CCACHE_COMPRESS:-1}"
  export CCACHE_SLOPPINESS="${CCACHE_SLOPPINESS:-time_macros,file_macro}"
  mkdir -p "$CCACHE_DIR"
}

native_mpv_common_args() {
  local prefix="$1"
  local build_type="$2"
  local cplayer="$3"
  MPV_NATIVE_ARGS=(
    --prefix "$prefix"
    --libdir lib
    --buildtype "$build_type"
    --default-library shared
    -Db_lto=false
    -Dbuild-date=false
    -Dlibmpv=true
    "-Dcplayer=$cplayer"
  )
}

clean_mpv_install_prefix() {
  local prefix="$1"
  rm -f "$prefix"/lib/libmpv.so*
  rm -f "$prefix/lib/pkgconfig/mpv.pc"
}

# mpv_meson_build SRC BUILD_DIR [meson setup args...]
#
# Configure + compile + install an mpv tree. Wipes BUILD_DIR first if it was
# previously configured against a different source path (meson bakes the source
# path into meson-info.json and refuses a plain --reconfigure across a move).
# The install --prefix must be passed in the setup args by the caller.
mpv_meson_build() {
  local src="$1" build="$2"
  shift 2
  local setup_args=("$@")

  if [[ -f "$build/meson-info/meson-info.json" ]]; then
    local cached_src
    cached_src="$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1])).get("directories",{}).get("source",""))' \
      "$build/meson-info/meson-info.json" 2>/dev/null || true)"
    if [[ -n "$cached_src" && "$cached_src" != "$src" ]]; then
      echo "mpv build dir cached with stale source path ($cached_src != $src); wiping" >&2
      rm -rf "$build"
    fi
  fi

  if [[ -f "$build/build.ninja" ]]; then
    meson setup --reconfigure "$build" "$src" "${setup_args[@]}"
  else
    meson setup "$build" "$src" "${setup_args[@]}"
  fi
  meson compile -C "$build"
  meson install -C "$build"
}

# cmake_build_app SRC BUILD_DIR [cmake configure args...]
#
# Configure (Ninja) + build + install the app. Wipes BUILD_DIR first if its
# CMakeCache.txt points at a different source tree, which otherwise makes CMake
# error out instead of reconfiguring.
cmake_build_app() {
  local src="$1" build="$2"
  shift 2
  local cmake_args=("$@")

  if [[ -f "$build/CMakeCache.txt" ]] \
     && ! grep -q "^CMAKE_HOME_DIRECTORY:INTERNAL=$src$" "$build/CMakeCache.txt"; then
    echo "app build dir cached with stale source path; wiping" >&2
    rm -rf "$build"
  fi
  mkdir -p "$build"

  cmake -S "$src" -B "$build" -GNinja "${cmake_args[@]}"
  cmake --build "$build" --parallel
  cmake --install "$build"
}

# resolve_qmlimportscanner [BUILD_NINJA]
#
# Print the path to a usable qmlimportscanner, or nothing if none is found.
# Nix splits Qt tools across outputs, so qmlimportscanner is rarely on PATH;
# both the AppImage packager (linuxdeploy-plugin-qt) and macdeployqt need it.
# Search order: PATH, qtpaths libexec dirs, CMAKE_PREFIX_PATH roots, the app's
# build.ninja (if given), then a last-resort /nix/store scan.
resolve_qmlimportscanner() {
  local build_ninja="${1:-}"
  local scanner
  scanner="$(command -v qmlimportscanner || true)"
  if [[ -n "$scanner" ]]; then
    printf '%s\n' "$scanner"
    return 0
  fi

  local roots=()
  if command -v qtpaths6 >/dev/null 2>&1; then
    roots+=("$(qtpaths6 -query QT_HOST_LIBEXECS 2>/dev/null || true)")
    roots+=("$(qtpaths6 -query QT_INSTALL_LIBEXECS 2>/dev/null || true)")
  elif command -v qtpaths >/dev/null 2>&1; then
    roots+=("$(qtpaths -query QT_HOST_LIBEXECS 2>/dev/null || true)")
    roots+=("$(qtpaths -query QT_INSTALL_LIBEXECS 2>/dev/null || true)")
  fi
  local cmake_roots=()
  IFS=':;' read -r -a cmake_roots <<< "${CMAKE_PREFIX_PATH:-}"
  roots+=("${cmake_roots[@]}")

  local root candidate
  for root in "${roots[@]}"; do
    [[ -n "$root" ]] || continue
    for candidate in \
      "$root/qmlimportscanner" \
      "$root/bin/qmlimportscanner" \
      "$root/libexec/qmlimportscanner" \
      "$root/lib/qt-6/libexec/qmlimportscanner"; do
      if [[ -x "$candidate" ]]; then
        printf '%s\n' "$candidate"
        return 0
      fi
    done
  done

  if [[ -n "$build_ninja" && -f "$build_ninja" ]]; then
    scanner="$(awk '
      /qmlimportscanner/ {
        for (i = 1; i <= NF; ++i) {
          gsub(/^"|"$/, "", $i)
          if ($i ~ /\/qmlimportscanner$/) { print $i; exit }
        }
      }' "$build_ninja")"
    if [[ -n "$scanner" ]]; then
      printf '%s\n' "$scanner"
      return 0
    fi
  fi

  # Last resort (Nix): match the scanner to the active Qt version so we never
  # hand back a stale, ABI-incompatible build from an unrelated store path. A
  # mismatched qmlimportscanner can run fine here yet fail under the mutated
  # LD_LIBRARY_PATH the packagers set up later, so an unpinned scan is unsafe.
  local qt_ver=""
  if command -v qtpaths6 >/dev/null 2>&1; then
    qt_ver="$(qtpaths6 -query QT_VERSION 2>/dev/null || true)"
  elif command -v qtpaths >/dev/null 2>&1; then
    qt_ver="$(qtpaths -query QT_VERSION 2>/dev/null || true)"
  fi
  if [[ -n "$qt_ver" ]]; then
    # A shell glob over the store's top level is bounded and fast; a recursive
    # `find /nix/store` would stat the entire (multi-hundred-GB) store.
    for candidate in /nix/store/*-qtdeclarative-"$qt_ver"*/libexec/qmlimportscanner; do
      if [[ -x "$candidate" ]]; then
        printf '%s\n' "$candidate"
        return 0
      fi
    done
  fi

  return 0
}
