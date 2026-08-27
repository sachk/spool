#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
source "$ROOT/tools/lib/build-common.sh"
source "$ROOT/tools/lib/manifest-sources.sh"

PHASE="${1:-all}"
ABI="${ANDROID_ABI:-x86_64}"
API="${ANDROID_API:-28}"
MANIFEST="${ANDROID_THIRD_PARTY_MANIFEST:-$ROOT/tools/manifests/android-third-party.json}"
JOBS="$(recommended_parallel_jobs "${ANDROID_DEPS_MEMORY_PER_JOB_MIB:-1024}" "${ANDROID_DEPS_MEMORY_RESERVE_MIB:-2048}")"

: "${ANDROID_NDK_ROOT:?run through nix develop .#android}"
HOST_TAG="linux-x86_64"
TOOLCHAIN="$ANDROID_NDK_ROOT/toolchains/llvm/prebuilt/$HOST_TAG"
[[ -d "$TOOLCHAIN" ]] || {
  echo "error: NDK LLVM toolchain missing at $TOOLCHAIN" >&2
  exit 1
}

case "$ABI" in
  arm64-v8a)
    TRIPLE=aarch64-linux-android
    CPU_FAMILY=aarch64
    CPU=aarch64
    FFMPEG_ARCH=aarch64
    OPENSSL_TARGET=android-arm64
    ;;
  x86_64)
    TRIPLE=x86_64-linux-android
    CPU_FAMILY=x86_64
    CPU=x86_64
    FFMPEG_ARCH=x86_64
    OPENSSL_TARGET=android-x86_64
    ;;
  *)
    echo "error: unsupported Android ABI: $ABI" >&2
    exit 1
    ;;
esac

CC="$TOOLCHAIN/bin/${TRIPLE}${API}-clang"
CXX="$TOOLCHAIN/bin/${TRIPLE}${API}-clang++"
AR="$TOOLCHAIN/bin/llvm-ar"
RANLIB="$TOOLCHAIN/bin/llvm-ranlib"
STRIP="$TOOLCHAIN/bin/llvm-strip"
NM="$TOOLCHAIN/bin/llvm-nm"
PREFIX="${ANDROID_DEPS_PREFIX:-$ROOT/build/android/deps/$ABI}"
SOURCE_ROOT="$ROOT/build/android/sources/third-party"
BUILD_ROOT="$ROOT/build/android/deps-build/$ABI"
CROSS_FILE="$BUILD_ROOT/android-$ABI.ini"

export PATH="$TOOLCHAIN/bin:$PATH"
export PKG_CONFIG_PATH="$PREFIX/lib/pkgconfig"
export PKG_CONFIG_LIBDIR="$PREFIX/lib/pkgconfig"
unset PKG_CONFIG_SYSROOT_DIR

fetch_sources() {
  mkdir -p "$SOURCE_ROOT" "$BUILD_ROOT"
  local source
  for source in openssl freetype lua ffmpeg qcoro fribidi harfbuzz libass libplacebo; do
    prepare_manifest_source "$ROOT" "$MANIFEST" "$source" "$SOURCE_ROOT/$source"
  done
  local path url sha archive
  while IFS=$'\t' read -r path url sha; do
    [[ -n "$path" ]] || continue
    archive="$ROOT/build/android/downloads/libplacebo-${path//\//-}-$sha.archive"
    download_verified "$url" "$sha" "$archive"
    extract_verified_source "$archive" "$sha" "$SOURCE_ROOT/libplacebo/$path"
  done < <(manifest_vendored_sources "$MANIFEST" libplacebo)
}

write_cross_file() {
  mkdir -p "$BUILD_ROOT" "$PREFIX"
  cat >"$CROSS_FILE" <<EOF
[binaries]
c = '$CC'
cpp = '$CXX'
ar = '$AR'
strip = '$STRIP'
nm = '$NM'
pkg-config = 'pkg-config'

[built-in options]
c_args = ['-fPIC']
cpp_args = ['-fPIC']
# mpv links with the C driver but pulls C++ code in from static harfbuzz and
# libplacebo, so the C++ runtime has to be requested explicitly or libmpv.so
# ends up with unresolved __gxx_personality_v0 at dlopen time.
c_link_args = ['-L$PREFIX/lib', '-lc++_shared']
cpp_link_args = ['-L$PREFIX/lib', '-lc++_shared']
pkg_config_path = ['$PREFIX/lib/pkgconfig']

[properties]
needs_exe_wrapper = true
pkg_config_libdir = ['$PREFIX/lib/pkgconfig']

[host_machine]
system = 'android'
cpu_family = '$CPU_FAMILY'
cpu = '$CPU'
endian = 'little'
EOF
}

build_openssl() {
  [[ -f "$PREFIX/lib/libssl.so" && -f "$PREFIX/lib/libcrypto.so" ]] && return
  local build="$BUILD_ROOT/openssl"
  rm -rf "$build"
  cp -R "$SOURCE_ROOT/openssl" "$build"
  chmod -R u+w "$build"
  (
    cd "$build"
    ./Configure "$OPENSSL_TARGET" -D__ANDROID_API__="$API" \
      --prefix="$PREFIX" --openssldir="$PREFIX/ssl" \
      shared no-tests no-ui-console
    make -j"$JOBS"
    make install_sw
  )
}

build_freetype() {
  [[ -f "$PREFIX/lib/pkgconfig/freetype2.pc" ]] && return
  local build="$BUILD_ROOT/freetype"
  rm -rf "$build"
  mkdir -p "$build"
  (
    cd "$build"
    "$SOURCE_ROOT/freetype/configure" \
      --host="$TRIPLE" --prefix="$PREFIX" \
      --enable-static --disable-shared \
      --with-zlib=no --with-bzip2=no --with-png=no --with-harfbuzz=no --with-brotli=no \
      CC="$CC" CXX="$CXX" AR="$AR" RANLIB="$RANLIB"
    make -j"$JOBS"
    make install
  )
}

# mpv's stats overlay, and every other builtin script, is Lua. Without it the
# player has no performance overlay at all. Lua ships a plain makefile rather
# than a cross-aware build, so point it at the NDK toolchain and write the
# pkg-config file mpv looks for.
build_lua() {
  [[ -f "$PREFIX/lib/pkgconfig/lua.pc" ]] && return
  local build="$BUILD_ROOT/lua"
  rm -rf "$build"
  cp -R "$SOURCE_ROOT/lua" "$build"
  chmod -R u+w "$build"
  make -C "$build/src" \
    CC="$CC -std=gnu99" \
    AR="$AR rcu" \
    RANLIB="$RANLIB" \
    MYCFLAGS="-fPIC" \
    SYSLIBS="-ldl -lm" \
    liblua.a
  install -Dm644 "$build/src/liblua.a" "$PREFIX/lib/liblua.a"
  local header
  for header in lua.h luaconf.h lualib.h lauxlib.h lua.hpp; do
    install -Dm644 "$build/src/$header" "$PREFIX/include/$header"
  done
  mkdir -p "$PREFIX/lib/pkgconfig"
  cat >"$PREFIX/lib/pkgconfig/lua.pc" <<EOF
prefix=$PREFIX
libdir=\${prefix}/lib
includedir=\${prefix}/include

Name: Lua
Description: Lua 5.2 scripting language
Version: $(manifest_source_field "$MANIFEST" lua version)
Libs: -L\${libdir} -llua -lm -ldl
Cflags: -I\${includedir}
EOF
}

build_meson() {
  local name="$1"
  shift
  [[ -f "$PREFIX/lib/pkgconfig/$name.pc" ]] && return
  local build="$BUILD_ROOT/$name"
  rm -rf "$build"
  meson setup "$build" "$SOURCE_ROOT/$name" \
    --cross-file "$CROSS_FILE" --prefix "$PREFIX" --default-library static "$@"
  meson compile -C "$build" -j "$JOBS"
  meson install -C "$build"
}

build_text_and_gpu_deps() {
  build_meson fribidi -Ddocs=false -Dbin=false -Dtests=false --buildtype release
  build_meson harfbuzz \
    -Dtests=disabled -Dutilities=disabled -Ddocs=disabled \
    -Dglib=disabled -Dgobject=disabled -Dcairo=disabled -Dchafa=disabled \
    -Dpng=disabled -Dzlib=disabled -Dicu=disabled -Dgraphite=disabled \
    -Dgraphite2=disabled -Dfreetype=enabled -Dfontations=disabled \
    -Dgdi=disabled -Ddirectwrite=disabled -Dcoretext=disabled \
    -Dharfrust=disabled -Dkbts=disabled -Dwasm=disabled -Draster=disabled \
    -Dvector=disabled -Dgpu=disabled -Dsubset=disabled --buildtype release
  build_meson libass \
    -Drequire-system-font-provider=false \
    -Dtest=disabled -Dcompare=disabled -Dprofile=disabled -Dfuzz=disabled \
    -Dcheckasm=disabled -Dfontconfig=disabled -Dlibunibreak=disabled \
    -Dasm=disabled --buildtype release
  build_meson libplacebo \
    -Ddemos=false -Dtests=false -Dbench=false -Dfuzz=false \
    -Dvulkan=disabled -Dvk-proc-addr=disabled -Dd3d11=disabled \
    -Dshaderc=disabled -Dglslang=disabled -Dlcms=disabled -Ddovi=disabled \
    -Dlibdovi=disabled -Dxxhash=disabled -Dopengl=enabled \
    -Dgl-proc-addr=enabled --buildtype release
}

build_ffmpeg() {
  [[ -f "$PREFIX/lib/pkgconfig/libavcodec.pc" ]] && return
  local build="$BUILD_ROOT/ffmpeg"
  rm -rf "$build"
  mkdir -p "$build"
  mapfile -t feature_flags < <(
    python3 "$ROOT/tools/ffmpeg-capabilities.py" \
      --manifest "$ROOT/tools/manifests/ffmpeg-capabilities.json" configure --platform android
  )
  (
    cd "$build"
    "$SOURCE_ROOT/ffmpeg/configure" \
      --prefix="$PREFIX" --target-os=android --arch="$FFMPEG_ARCH" \
      --enable-cross-compile --sysroot="$TOOLCHAIN/sysroot" \
      --cc="$CC" --cxx="$CXX" --ar="$AR" --ranlib="$RANLIB" --strip="$STRIP" --nm="$NM" \
      --pkg-config=pkg-config --enable-static --disable-shared --enable-pic \
      --extra-cflags="-I$PREFIX/include" \
      --extra-ldflags="-L$PREFIX/lib" \
      "${feature_flags[@]}"
    make -j"$JOBS"
    make install
  )
}

build_mpv() {
  [[ -f "$PREFIX/lib/libmpv.so" ]] && return
  local build="$BUILD_ROOT/mpv"
  rm -rf "$build"
  meson setup "$build" "$ROOT/mpv" \
    --cross-file "$CROSS_FILE" --prefix "$PREFIX" --default-library shared \
    -Dcplayer=false -Dlibmpv=true -Dbuild-date=false -Dtests=false \
    -Dlua=lua -Djavascript=disabled -Dmanpage-build=disabled \
    -Dlibarchive=disabled -Dlibbluray=disabled -Dlibcurl=disabled \
    -Dgl=enabled -Degl=disabled -Degl-android=enabled -Dvulkan=disabled \
    -Dandroid-media-ndk=enabled -Daudiotrack=enabled -Daaudio=enabled
  meson compile -C "$build" -j "$JOBS"
  meson install -C "$build"
}

build_all() {
  fetch_sources
  write_cross_file
  describe_parallel_jobs "$JOBS" "Android dependencies" "${ANDROID_DEPS_MEMORY_PER_JOB_MIB:-1024}" "${ANDROID_DEPS_MEMORY_RESERVE_MIB:-2048}"
  build_openssl
  build_freetype
  build_lua
  build_text_and_gpu_deps
  build_ffmpeg
  build_mpv
  printf 'Android dependencies installed at %s\n' "$PREFIX"
}

case "$PHASE" in
  fetch) fetch_sources ;;
  build | all) build_all ;;
  clean) rm -rf "$BUILD_ROOT" "$PREFIX" ;;
  *)
    echo "usage: $0 [fetch|build|all|clean]" >&2
    exit 2
    ;;
esac
