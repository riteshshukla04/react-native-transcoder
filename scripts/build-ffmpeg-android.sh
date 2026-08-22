#!/bin/bash
# Cross-builds the pinned LGPL FFmpeg audio profile for Android.
# Output: packages/react-native-transcoder/dependencies/prebuilt/android/<abi>/{lib,include}
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
FFMPEG_VERSION=7.1.1
FFMPEG_SHA256=733984395e0dbbe5c046abda2dc49a5544e7e0e1e2366bba849222ae9e3a03b1
ABIS=${ABIS:-"arm64-v8a armeabi-v7a x86_64 x86"}
MIN_SDK=${MIN_SDK:-23}
NDK=${ANDROID_NDK_HOME:-${ANDROID_HOME:-$HOME/Library/Android/sdk}/ndk/28.2.13676358}

WORK_DIR="${FFMPEG_WORK_DIR:-$REPO_ROOT/packages/react-native-transcoder/dependencies/build}"
PREBUILT_DIR="$REPO_ROOT/packages/react-native-transcoder/dependencies/prebuilt/android"
SRC_DIR="$WORK_DIR/ffmpeg-$FFMPEG_VERSION"

HOST_TAG=$(uname -s | tr '[:upper:]' '[:lower:]')-x86_64
TOOLCHAIN="$NDK/toolchains/llvm/prebuilt/$HOST_TAG"
if [ ! -d "$TOOLCHAIN" ]; then
  TOOLCHAIN="$NDK/toolchains/llvm/prebuilt/darwin-x86_64"
fi
test -d "$TOOLCHAIN" || { echo "NDK toolchain not found at $TOOLCHAIN" >&2; exit 1; }

# Wave A audio only. Every entry here must have a row in the capability manifest.
COMPONENTS=(
  --enable-demuxer=wav,aiff,caf,mov,mp3,flac,ogg,matroska,aac,w64,pcm_s16le,pcm_f32le
  --enable-muxer=wav,aiff,caf,ipod,mp4,mp3,flac,ogg,matroska,adts,w64
  --enable-decoder=pcm_s16le,pcm_s16be,pcm_s24le,pcm_s32le,pcm_f32le,pcm_f64le,pcm_u8,pcm_alaw,pcm_mulaw,aac,mp3,mp3float,flac,alac,vorbis,opus
  --enable-encoder=pcm_s16le,pcm_s16be,pcm_s24le,pcm_s32le,pcm_f32le,pcm_u8,aac,flac,alac
  --enable-parser=aac,aac_latm,flac,mpegaudio,vorbis,opus
  --enable-bsf=aac_adtstoasc,extract_extradata
)

mkdir -p "$WORK_DIR"
if [ ! -d "$SRC_DIR" ]; then
  echo "==> downloading ffmpeg $FFMPEG_VERSION"
  curl -fsSL "https://ffmpeg.org/releases/ffmpeg-$FFMPEG_VERSION.tar.xz" -o "$WORK_DIR/ffmpeg.tar.xz"
  echo "$FFMPEG_SHA256  $WORK_DIR/ffmpeg.tar.xz" | shasum -a 256 -c -
  tar -xf "$WORK_DIR/ffmpeg.tar.xz" -C "$WORK_DIR"
  rm -f "$WORK_DIR/ffmpeg.tar.xz"
fi

for ABI in $ABIS; do
  EXTRA_CFLAGS=""
  EXTRA_CONFIGURE=""
  case "$ABI" in
    arm64-v8a)   ARCH=aarch64; TRIPLE=aarch64-linux-android;    CPU=armv8-a ;;
    armeabi-v7a) ARCH=arm;     TRIPLE=armv7a-linux-androideabi; CPU=armv7-a; EXTRA_CFLAGS="-mfpu=neon -mfloat-abi=softfp" ;;
    x86_64)      ARCH=x86_64;  TRIPLE=x86_64-linux-android;     CPU=x86-64 ;;
    # 32-bit x86 asm is not PIC-safe in a shared build; the emulator-only ABI takes the C path.
    x86)         ARCH=x86;     TRIPLE=i686-linux-android;       CPU=i686; EXTRA_CONFIGURE="--disable-asm" ;;
    *) echo "unsupported ABI: $ABI" >&2; exit 1 ;;
  esac

  BUILD_DIR="$WORK_DIR/build-android-$ABI"
  OUT_DIR="$PREBUILT_DIR/$ABI"
  rm -rf "$BUILD_DIR" "$OUT_DIR"
  mkdir -p "$BUILD_DIR"

  echo "==> configuring ffmpeg for $ABI"
  (
    cd "$BUILD_DIR"
    "$SRC_DIR/configure" \
      --prefix="$OUT_DIR" \
      --target-os=android \
      --arch="$ARCH" \
      --cpu="$CPU" \
      --enable-cross-compile \
      --cross-prefix="$TOOLCHAIN/bin/llvm-" \
      --cc="$TOOLCHAIN/bin/${TRIPLE}${MIN_SDK}-clang" \
      --cxx="$TOOLCHAIN/bin/${TRIPLE}${MIN_SDK}-clang++" \
      --ld="$TOOLCHAIN/bin/${TRIPLE}${MIN_SDK}-clang" \
      --ar="$TOOLCHAIN/bin/llvm-ar" \
      --nm="$TOOLCHAIN/bin/llvm-nm" \
      --ranlib="$TOOLCHAIN/bin/llvm-ranlib" \
      --strip="$TOOLCHAIN/bin/llvm-strip" \
      --sysroot="$TOOLCHAIN/sysroot" \
      --extra-cflags="$EXTRA_CFLAGS" \
      --extra-ldflags="-Wl,-z,max-page-size=16384 -Wl,-z,common-page-size=16384" \
      --enable-shared \
      --disable-static \
      --enable-pic \
      --enable-small \
      --disable-gpl \
      --disable-nonfree \
      --disable-programs \
      --disable-doc \
      --disable-avdevice \
      --disable-avfilter \
      --disable-postproc \
      --disable-swscale \
      --disable-network \
      --disable-protocols \
      --disable-devices \
      --disable-everything \
      --disable-debug \
      --disable-symver \
      ${EXTRA_CONFIGURE} \
      "${COMPONENTS[@]}"

    make -j"$(sysctl -n hw.ncpu 2>/dev/null || nproc)"
    make install
  )

  # Stage the shared libraries in the jniLibs layout Gradle expects.
  JNI_DIR="$PREBUILT_DIR/jniLibs/$ABI"
  mkdir -p "$JNI_DIR"
  rm -f "$JNI_DIR"/*.so
  cp "$OUT_DIR/lib"/*.so "$JNI_DIR/"

  echo "==> built $ABI:"
  ls "$JNI_DIR"/*.so
done
