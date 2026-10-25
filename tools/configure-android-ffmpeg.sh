#!/bin/sh
set -eu
source_dir=$1
prefix_dir=$2
ndk_dir=$3
case "${4:-arm64-v8a}" in
  arm64-v8a) arch=aarch64; compiler=aarch64-linux-android26 ;;
  x86_64) arch=x86_64; compiler=x86_64-linux-android26 ;;
  *) echo 'Unsupported Android ABI' >&2; exit 1 ;;
esac
case "$(uname -s)" in
  Darwin) host_tag=darwin-x86_64 ;;
  Linux) host_tag=linux-x86_64 ;;
  *) echo 'Unsupported FFmpeg build host' >&2; exit 1 ;;
esac
toolchain="$ndk_dir/toolchains/llvm/prebuilt/$host_tag"
exec "$source_dir/configure" --prefix="$prefix_dir" --target-os=android --arch="$arch" \
  --enable-cross-compile --sysroot="$toolchain/sysroot" \
  --cc="$toolchain/bin/${compiler}-clang" \
  --cxx="$toolchain/bin/${compiler}-clang++" \
  --ar="$toolchain/bin/llvm-ar" --ranlib="$toolchain/bin/llvm-ranlib" \
  --nm="$toolchain/bin/llvm-nm" --strip="$toolchain/bin/llvm-strip" \
  --extra-ldflags=-Wl,-z,max-page-size=16384 \
  --enable-shared --disable-static --disable-programs --disable-doc \
  --disable-x86asm --disable-autodetect --disable-everything --disable-avdevice --disable-avfilter \
  --disable-swscale --disable-network --enable-avformat --enable-avcodec --enable-swresample \
  --enable-protocol=file --enable-demuxer=asf,wav \
  --enable-decoder=wmav1,wmav2,wmapro,wmalossless,wmavoice,pcm_s16le,adpcm_ms
