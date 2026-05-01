#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
SDK_ROOT="${WEBOS_SDK_ROOT:-$ROOT/build/webos-sdk/arm-webos-linux-gnueabi_sdk-buildroot}"
SDK_BIN="$SDK_ROOT/bin"
SYSROOT="$SDK_ROOT/arm-webos-linux-gnueabi/sysroot"
TARGET_PREFIX="${WEBOS_TARGET_PREFIX:-/usr/local/webos-native}"
PREFIX="${WEBOS_NATIVE_PREFIX:-$SYSROOT$TARGET_PREFIX}"
SRC_ARCHIVE="${FFMPEG_ARCHIVE:-$ROOT/experiments/optionc-webos-ipk/vendor/ffmpeg-8.1.tar.xz}"
SRC_DIR="${FFMPEG_SRC_DIR:-$ROOT/build/ffmpeg-src}"
BUILD_DIR="${FFMPEG_BUILD_DIR:-$ROOT/build/ffmpeg-build}"

mkdir -p "$ROOT/build"

if [[ ! -f "$SRC_ARCHIVE" ]]; then
  printf 'Missing FFmpeg source archive: %s\n' "$SRC_ARCHIVE" >&2
  exit 1
fi

rm -rf "$SRC_DIR" "$BUILD_DIR"
mkdir -p "$SRC_DIR" "$BUILD_DIR"
tar -xf "$SRC_ARCHIVE" -C "$SRC_DIR" --strip-components=1

export PKG_CONFIG_LIBDIR="$PREFIX/lib/pkgconfig:$SYSROOT/usr/lib/pkgconfig:$SYSROOT/usr/share/pkgconfig"
export PKG_CONFIG_SYSROOT_DIR="$SYSROOT"

cd "$SRC_DIR"

./configure \
  --prefix="$TARGET_PREFIX" \
  --arch=arm \
  --cpu=cortex-a9 \
  --target-os=linux \
  --enable-cross-compile \
  --cross-prefix="$SDK_BIN/arm-webos-linux-gnueabi-" \
  --cc="$SDK_BIN/arm-webos-linux-gnueabi-gcc" \
  --cxx="$SDK_BIN/arm-webos-linux-gnueabi-g++" \
  --ar="$SDK_BIN/arm-webos-linux-gnueabi-ar" \
  --ranlib="$SDK_BIN/arm-webos-linux-gnueabi-ranlib" \
  --strip="$SDK_BIN/arm-webos-linux-gnueabi-strip" \
  --pkg-config="$SDK_BIN/pkg-config" \
  --disable-static \
  --enable-shared \
  --enable-pic \
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
  --extra-cflags="--sysroot=$SYSROOT -I$PREFIX/include" \
  --extra-ldflags="--sysroot=$SYSROOT -L$PREFIX/lib -Wl,-rpath-link,$PREFIX/lib"

make -j"$(getconf _NPROCESSORS_ONLN)"
make DESTDIR="$SYSROOT" install

printf '\nInstalled FFmpeg into %s\n' "$PREFIX"
