#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
# shellcheck source=tools/lib/build-common.sh
source "$ROOT/tools/lib/build-common.sh"
# shellcheck source=tools/lib/manifest-sources.sh
source "$ROOT/tools/lib/manifest-sources.sh"
MANIFEST="${WEBOS_THIRD_PARTY_MANIFEST:-$ROOT/tools/manifests/webos-third-party.json}"
SDK_ROOT="${WEBOS_SDK_ROOT:-$ROOT/build/webos-sdk/arm-webos-linux-gnueabi_sdk-buildroot}"
SDK_BIN="$SDK_ROOT/bin"
SYSROOT="$SDK_ROOT/arm-webos-linux-gnueabi/sysroot"
TARGET_PREFIX="${WEBOS_TARGET_PREFIX:-/usr/local/webos-native}"
PREFIX="${WEBOS_NATIVE_PREFIX:-$SYSROOT$TARGET_PREFIX}"
FFMPEG_URL="${FFMPEG_URL:-$(manifest_source_field "$MANIFEST" ffmpeg url)}"
FFMPEG_SHA256="${FFMPEG_SHA256:-$(manifest_source_field "$MANIFEST" ffmpeg sha256)}"
SRC_ARCHIVE="${FFMPEG_ARCHIVE:-$ROOT/build/downloads/ffmpeg-8.1.tar.xz}"
SRC_DIR="${FFMPEG_SRC_DIR:-$ROOT/build/ffmpeg-src}"
BUILD_DIR="${FFMPEG_BUILD_DIR:-$ROOT/build/ffmpeg-build}"
WEBOS_BUILD_MEMORY_PER_JOB_MIB="${WEBOS_BUILD_MEMORY_PER_JOB_MIB:-1536}"
WEBOS_BUILD_MEMORY_RESERVE_MIB="${WEBOS_BUILD_MEMORY_RESERVE_MIB:-2048}"
WEBOS_BUILD_JOBS="$(recommended_parallel_jobs "$WEBOS_BUILD_MEMORY_PER_JOB_MIB" "$WEBOS_BUILD_MEMORY_RESERVE_MIB")"
FFMPEG_PGO_FLAGS="$(webos_pgo_flags FFMPEG "$ROOT/build/pgo/ffmpeg")"
# Keep unwind tables and debug information out of release libraries unless an
# instrumented build explicitly supplies FFMPEG_DIAG_CFLAGS.
FFMPEG_DIAG_CFLAGS="${FFMPEG_DIAG_CFLAGS:-}"

mkdir -p "$ROOT/build"

download_verified "$FFMPEG_URL" "$FFMPEG_SHA256" "$SRC_ARCHIVE"

rm -rf "$SRC_DIR" "$BUILD_DIR"
mkdir -p "$SRC_DIR" "$BUILD_DIR"
tar -xf "$SRC_ARCHIVE" -C "$SRC_DIR" --strip-components=1

export PKG_CONFIG_LIBDIR="$PREFIX/lib/pkgconfig:$SYSROOT/usr/lib/pkgconfig:$SYSROOT/usr/share/pkgconfig"
export PKG_CONFIG_SYSROOT_DIR="$SYSROOT"

cd "$SRC_DIR"
describe_parallel_jobs "$WEBOS_BUILD_JOBS" "FFmpeg" "$WEBOS_BUILD_MEMORY_PER_JOB_MIB" "$WEBOS_BUILD_MEMORY_RESERVE_MIB"

./configure \
  --prefix="$TARGET_PREFIX" \
  --arch=arm \
  --cpu=cortex-a53 \
  --enable-thumb \
  --target-os=linux \
  --enable-cross-compile \
  --cross-prefix="$SDK_BIN/arm-webos-linux-gnueabi-" \
  --cc="$SDK_BIN/arm-webos-linux-gnueabi-gcc" \
  --cxx="$SDK_BIN/arm-webos-linux-gnueabi-g++" \
  --ar="$SDK_BIN/arm-webos-linux-gnueabi-gcc-ar" \
  --ranlib="$SDK_BIN/arm-webos-linux-gnueabi-gcc-ranlib" \
  --strip="$SDK_BIN/arm-webos-linux-gnueabi-strip" \
  --pkg-config="$SDK_BIN/pkg-config" \
  --disable-static \
  --enable-shared \
  --enable-pic \
  --enable-lto \
  --disable-programs \
  --disable-doc \
  --disable-debug \
  --disable-autodetect \
  --disable-avdevice \
  --disable-iconv \
  --enable-optimizations \
  --enable-avcodec \
  --enable-avfilter \
  --enable-avformat \
  --enable-avutil \
  --enable-swresample \
  --enable-swscale \
  --enable-network \
  --enable-protocol=file \
  --enable-protocol=http \
  --enable-protocol=https \
  --enable-protocol=tcp \
  --enable-protocol=tls \
  --enable-protocol=pipe \
  --enable-demuxer=matroska \
  --enable-demuxer=mov \
  --enable-demuxer=mp3 \
  --enable-demuxer=flac \
  --enable-demuxer=wav \
  --enable-demuxer=ogg \
  --enable-demuxer=mpegts \
  --enable-demuxer=avi \
  --enable-demuxer=aac \
  --enable-parser=aac \
  --enable-parser=ac3 \
  --enable-parser=dca \
  --enable-parser=eac3 \
  --enable-parser=flac \
  --enable-parser=h264 \
  --enable-parser=hevc \
  --enable-parser=mlp \
  --enable-parser=mpegaudio \
  --enable-parser=opus \
  --enable-parser=vorbis \
  --enable-decoder=aac \
  --enable-decoder=ac3 \
  --enable-decoder=alac \
  --enable-decoder=dca \
  --enable-decoder=eac3 \
  --enable-decoder=flac \
  --enable-decoder=mlp \
  --enable-decoder=mp2 \
  --enable-decoder=mp3 \
  --enable-decoder=opus \
  --enable-decoder=pcm_bluray \
  --enable-decoder=pcm_f32le \
  --enable-decoder=pcm_s16be \
  --enable-decoder=pcm_s16le \
  --enable-decoder=pcm_s24be \
  --enable-decoder=pcm_s24le \
  --enable-decoder=pcm_s32le \
  --enable-decoder=truehd \
  --enable-decoder=vorbis \
  --enable-decoder=wmav2 \
  --enable-decoder=wmapro \
  --enable-encoder=aac \
  --enable-muxer=spdif \
  --enable-bsf=aac_adtstoasc \
  --enable-bsf=h264_mp4toannexb \
  --enable-bsf=hevc_mp4toannexb \
  --extra-cflags="--sysroot=$SYSROOT -I$PREFIX/include $(webos_tune_cflags) $FFMPEG_DIAG_CFLAGS $FFMPEG_PGO_FLAGS" \
  --extra-ldflags="--sysroot=$SYSROOT -L$PREFIX/lib -Wl,-rpath-link,$PREFIX/lib $FFMPEG_PGO_FLAGS"

make -j"$WEBOS_BUILD_JOBS"
make DESTDIR="$SYSROOT" install

printf '\nInstalled FFmpeg into %s\n' "$PREFIX"
