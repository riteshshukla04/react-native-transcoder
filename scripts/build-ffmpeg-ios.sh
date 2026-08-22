#!/bin/bash
# Cross-builds the pinned LGPL FFmpeg audio profile for iOS as dynamic
# frameworks, then packages them as XCFrameworks so the LGPL components stay
# replaceable inside the app bundle.
# Output: packages/react-native-transcoder/dependencies/prebuilt/ios/*.xcframework
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
FFMPEG_VERSION=7.1.1
FFMPEG_SHA256=733984395e0dbbe5c046abda2dc49a5544e7e0e1e2366bba849222ae9e3a03b1
MIN_IOS=${MIN_IOS:-15.1}
LIBS=(avutil swresample avcodec avformat)

WORK_DIR="${FFMPEG_WORK_DIR:-$REPO_ROOT/packages/react-native-transcoder/dependencies/build}"
OUT_ROOT="$REPO_ROOT/packages/react-native-transcoder/dependencies/prebuilt/ios"
SRC_DIR="$WORK_DIR/ffmpeg-$FFMPEG_VERSION"

COMPONENTS=(
  --enable-demuxer=wav,aiff,caf,mov,mp3,flac,ogg,matroska,aac,w64,pcm_s16le,pcm_f32le
  --enable-muxer=wav,aiff,caf,ipod,mp4,mp3,flac,ogg,matroska,adts,w64
  --enable-decoder=pcm_s8,pcm_s16le,pcm_s16be,pcm_s24le,pcm_s24be,pcm_s32le,pcm_s32be,pcm_f32le,pcm_f32be,pcm_f64le,pcm_f64be,pcm_u8,pcm_alaw,pcm_mulaw,aac,mp3,mp3float,flac,alac,vorbis,opus
  --enable-encoder=pcm_s16le,pcm_s16be,pcm_s24le,pcm_s32le,pcm_f32le,pcm_u8,aac,flac,alac,libmp3lame,libopus,libvorbis
  --enable-parser=aac,aac_latm,flac,mpegaudio,vorbis,opus
  --enable-bsf=aac_adtstoasc,extract_extradata
  --enable-libmp3lame
  --enable-libopus
  --enable-libvorbis
)

mkdir -p "$WORK_DIR"
if [ ! -d "$SRC_DIR" ]; then
  echo "==> downloading ffmpeg $FFMPEG_VERSION"
  curl -fsSL "https://ffmpeg.org/releases/ffmpeg-$FFMPEG_VERSION.tar.xz" -o "$WORK_DIR/ffmpeg.tar.xz"
  echo "$FFMPEG_SHA256  $WORK_DIR/ffmpeg.tar.xz" | shasum -a 256 -c -
  tar -xf "$WORK_DIR/ffmpeg.tar.xz" -C "$WORK_DIR"
  rm -f "$WORK_DIR/ffmpeg.tar.xz"
fi

build_slice() {
  local NAME=$1 SDK=$2 ARCH=$3 MIN_FLAG=$4
  local BUILD_DIR="$WORK_DIR/build-ios-$NAME"
  local PREFIX="$WORK_DIR/install-ios-$NAME"
  if [ "${REUSE_SLICES:-0}" = "1" ] && [ -f "$PREFIX/lib/libavcodec.dylib" ]; then
    echo "==> reusing existing $NAME slice"
    return
  fi
  local SYSROOT
  SYSROOT=$(xcrun --sdk "$SDK" --show-sdk-path)

  rm -rf "$BUILD_DIR" "$PREFIX"
  mkdir -p "$BUILD_DIR"

  # LAME, Opus and Vorbis are static archives linked into the shared FFmpeg libraries.
  # The triple must say aarch64: naming it arm-apple-darwin makes Opus build its
  # 32-bit ARM assembly, which Apple's assembler rejects.
  local CODEC_PREFIX="$WORK_DIR/codec-ios-$NAME"
  local CODEC_HOST=x86_64-apple-darwin
  [ "$ARCH" = "arm64" ] && CODEC_HOST=aarch64-apple-darwin
  CODEC_PREFIX="$CODEC_PREFIX" \
  CODEC_HOST="$CODEC_HOST" \
  CODEC_WORK_DIR="$WORK_DIR" \
  CC="$(xcrun -f clang)" \
  CFLAGS="-arch $ARCH -isysroot $SYSROOT $MIN_FLAG -fPIC -Os" \
  LDFLAGS="-arch $ARCH -isysroot $SYSROOT $MIN_FLAG" \
    "$REPO_ROOT/scripts/build-codec-libs.sh"

  echo "==> configuring ffmpeg for $NAME"
  (
    cd "$BUILD_DIR"
    # Restrict pkg-config to the cross prefix so libopus resolves there, never on the host.
    export PKG_CONFIG_LIBDIR="$CODEC_PREFIX/lib/pkgconfig"
    "$SRC_DIR/configure" \
      --prefix="$PREFIX" \
      --target-os=darwin \
      --arch="$ARCH" \
      --enable-cross-compile \
      --cc="$(xcrun -f clang)" \
      --as="$(xcrun -f clang)" \
      --sysroot="$SYSROOT" \
      --extra-cflags="-arch $ARCH -isysroot $SYSROOT $MIN_FLAG -fembed-bitcode-marker -I$CODEC_PREFIX/include" \
      --extra-ldflags="-arch $ARCH -isysroot $SYSROOT $MIN_FLAG -L$CODEC_PREFIX/lib" \
      --pkg-config-flags=--static \
      --install-name-dir='@rpath' \
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
      --disable-coreimage \
      --disable-audiotoolbox \
      "${COMPONENTS[@]}"

    make -j"$(sysctl -n hw.ncpu)"
    make install
  )
}

make_framework() {
  local LIB=$1 SLICE_DIR=$2 OUT_DIR=$3 PLATFORM=$4
  local FRAMEWORK="$OUT_DIR/$LIB.framework"
  rm -rf "$FRAMEWORK"
  mkdir -p "$FRAMEWORK"

  # The unversioned symlink always points at the real dylib for this slice.
  cp "$SLICE_DIR/lib/lib$LIB.dylib" "$FRAMEWORK/$LIB"
  chmod +w "$FRAMEWORK/$LIB"

  install_name_tool -id "@rpath/$LIB.framework/$LIB" "$FRAMEWORK/$LIB"
  for DEP in "${LIBS[@]}"; do
    OLD_PATHS=$(otool -L "$FRAMEWORK/$LIB" | awk '{print $1}' | grep "lib$DEP\." || true)
    for OLD in $OLD_PATHS; do
      install_name_tool -change "$OLD" "@rpath/$DEP.framework/$DEP" "$FRAMEWORK/$LIB"
    done
  done

  cat > "$FRAMEWORK/Info.plist" <<PLIST
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
  <key>CFBundleDevelopmentRegion</key><string>en</string>
  <key>CFBundleExecutable</key><string>$LIB</string>
  <key>CFBundleIdentifier</key><string>org.ffmpeg.$LIB</string>
  <key>CFBundleInfoDictionaryVersion</key><string>6.0</string>
  <key>CFBundleName</key><string>$LIB</string>
  <key>CFBundlePackageType</key><string>FMWK</string>
  <key>CFBundleShortVersionString</key><string>$FFMPEG_VERSION</string>
  <key>CFBundleVersion</key><string>$FFMPEG_VERSION</string>
  <key>MinimumOSVersion</key><string>$MIN_IOS</string>
  <key>CFBundleSupportedPlatforms</key><array><string>$PLATFORM</string></array>
</dict>
</plist>
PLIST
}

build_slice device       iphoneos        arm64  "-mios-version-min=$MIN_IOS"
build_slice simulator-arm64  iphonesimulator arm64  "-mios-simulator-version-min=$MIN_IOS"
build_slice simulator-x86_64 iphonesimulator x86_64 "-mios-simulator-version-min=$MIN_IOS"

# Fuse the two simulator architectures into one slice.
FAT_SIM="$WORK_DIR/install-ios-simulator"
rm -rf "$FAT_SIM"
mkdir -p "$FAT_SIM/lib"
cp -R "$WORK_DIR/install-ios-simulator-arm64/include" "$FAT_SIM/include"
for LIB in "${LIBS[@]}"; do
  lipo -create "$WORK_DIR/install-ios-simulator-arm64/lib/lib$LIB.dylib" \
       "$WORK_DIR/install-ios-simulator-x86_64/lib/lib$LIB.dylib" \
       -output "$FAT_SIM/lib/lib$LIB.dylib"
done

rm -rf "$OUT_ROOT"
mkdir -p "$OUT_ROOT" "$WORK_DIR/frameworks/device" "$WORK_DIR/frameworks/simulator"
rm -rf "$WORK_DIR/frameworks"
mkdir -p "$WORK_DIR/frameworks/device" "$WORK_DIR/frameworks/simulator"

for LIB in "${LIBS[@]}"; do
  make_framework "$LIB" "$WORK_DIR/install-ios-device" "$WORK_DIR/frameworks/device" iPhoneOS
  make_framework "$LIB" "$FAT_SIM" "$WORK_DIR/frameworks/simulator" iPhoneSimulator
  xcodebuild -create-xcframework \
    -framework "$WORK_DIR/frameworks/device/$LIB.framework" \
    -framework "$WORK_DIR/frameworks/simulator/$LIB.framework" \
    -output "$OUT_ROOT/$LIB.xcframework" > /dev/null
done

# Headers are identical across slices apart from avconfig/ffversion, which match
# for a single pinned configuration.
rm -rf "$OUT_ROOT/include"
cp -R "$WORK_DIR/install-ios-device/include" "$OUT_ROOT/include"

echo "==> built iOS XCFrameworks:"
ls -d "$OUT_ROOT"/*.xcframework
