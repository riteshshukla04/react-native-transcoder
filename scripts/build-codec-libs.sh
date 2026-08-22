#!/bin/bash
# Cross-builds the external audio encoder libraries FFmpeg links against:
# LAME (MP3), Opus, and Vorbis (plus its Ogg dependency).
#
# These are built as static archives and end up inside the shared FFmpeg
# libraries, which stay dynamically linked and replaceable on both platforms.
#
# Driven by build-ffmpeg-android.sh and build-ffmpeg-ios.sh. Required:
#   CODEC_PREFIX  install prefix (headers, static libs, pkgconfig)
#   CODEC_HOST    autotools --host triple
#   CC            cross compiler
# Optional: CFLAGS, LDFLAGS, AR, RANLIB, NM, STRIP, CODEC_WORK_DIR, FORCE_CODECS
set -euo pipefail

: "${CODEC_PREFIX:?CODEC_PREFIX is required}"
: "${CODEC_HOST:?CODEC_HOST is required}"
: "${CC:?CC is required}"

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
WORK_DIR="${CODEC_WORK_DIR:-$REPO_ROOT/packages/react-native-transcoder/dependencies/build}"
JOBS=$(sysctl -n hw.ncpu 2>/dev/null || nproc)

OGG_VERSION=1.3.5
OGG_SHA256=0eb4b4b9420a0f51db142ba3f9c64b333f826532dc0f48c6410ae51f4799b664
OGG_URL="https://downloads.xiph.org/releases/ogg/libogg-$OGG_VERSION.tar.gz"

VORBIS_VERSION=1.3.7
VORBIS_SHA256=0e982409a9c3fc82ee06e08205b1355e5c6aa4c36bca58146ef399621b0ce5ab
VORBIS_URL="https://downloads.xiph.org/releases/vorbis/libvorbis-$VORBIS_VERSION.tar.gz"

OPUS_VERSION=1.5.2
OPUS_SHA256=65c1d2f78b9f2fb20082c38cbe47c951ad5839345876e46941612ee87f9a7ce1
OPUS_URL="https://downloads.xiph.org/releases/opus/opus-$OPUS_VERSION.tar.gz"

LAME_VERSION=3.100
LAME_SHA256=ddfe36cab873794038ae2c1210557ad34857a4b6bdc515785d1da9e175b1da1e
LAME_URL="https://downloads.sourceforge.net/project/lame/lame/$LAME_VERSION/lame-$LAME_VERSION.tar.gz"

if [ "${FORCE_CODECS:-0}" != "1" ] && [ -f "$CODEC_PREFIX/lib/libmp3lame.a" ] &&
  [ -f "$CODEC_PREFIX/lib/libopus.a" ] && [ -f "$CODEC_PREFIX/lib/libvorbisenc.a" ]; then
  echo "==> reusing codec libraries in $CODEC_PREFIX"
  exit 0
fi

mkdir -p "$WORK_DIR" "$CODEC_PREFIX"

fetch() {
  local NAME=$1 URL=$2 SHA=$3
  local SRC="$WORK_DIR/$NAME"
  [ -d "$SRC" ] && return
  echo "==> downloading $NAME"
  curl -fsSL "$URL" -o "$WORK_DIR/$NAME.tar.gz"
  echo "$SHA  $WORK_DIR/$NAME.tar.gz" | shasum -a 256 -c -
  tar -xf "$WORK_DIR/$NAME.tar.gz" -C "$WORK_DIR"
  rm -f "$WORK_DIR/$NAME.tar.gz"
}

# Out-of-tree builds keep one extracted source usable by every architecture.
build_autotools() {
  local NAME=$1
  shift
  local SRC="$WORK_DIR/$NAME"
  local BUILD="$WORK_DIR/codec-build-$(basename "$CODEC_PREFIX")-$NAME"
  rm -rf "$BUILD"
  mkdir -p "$BUILD"
  echo "==> building $NAME for $CODEC_HOST"
  (
    cd "$BUILD"
    "$SRC/configure" \
      --prefix="$CODEC_PREFIX" \
      --host="$CODEC_HOST" \
      --disable-shared \
      --enable-static \
      --with-pic \
      "$@"
    make -j"$JOBS"
    make install
  )
}

fetch "libogg-$OGG_VERSION" "$OGG_URL" "$OGG_SHA256"
fetch "libvorbis-$VORBIS_VERSION" "$VORBIS_URL" "$VORBIS_SHA256"
fetch "opus-$OPUS_VERSION" "$OPUS_URL" "$OPUS_SHA256"
fetch "lame-$LAME_VERSION" "$LAME_URL" "$LAME_SHA256"

# `sed -i` takes its backup suffix differently on BSD and GNU; Android CI is Linux.
patch_in_place() {
  local FILE=$1
  shift
  sed "$@" "$FILE" > "$FILE.patched"
  mv "$FILE.patched" "$FILE"
}

# libvorbis hard-codes -mno-ieee-fp on i*86 and -force_cpusubtype_ALL on darwin;
# current clang and ld reject both.
patch_in_place "$WORK_DIR/libvorbis-$VORBIS_VERSION/configure" \
  -e 's/-mno-ieee-fp//g' -e 's/-force_cpusubtype_ALL//g'
chmod +x "$WORK_DIR/libvorbis-$VORBIS_VERSION/configure"
# lame 3.100 exports a symbol its own sources no longer define.
patch_in_place "$WORK_DIR/lame-$LAME_VERSION/include/libmp3lame.sym" -e '/lame_init_old/d'

build_autotools "libogg-$OGG_VERSION"
build_autotools "libvorbis-$VORBIS_VERSION" --disable-oggtest --with-ogg="$CODEC_PREFIX"
build_autotools "opus-$OPUS_VERSION" --disable-doc --disable-extra-programs
build_autotools "lame-$LAME_VERSION" --disable-frontend --disable-decoder --disable-analyzer-hooks

echo "==> codec libraries in $CODEC_PREFIX:"
ls "$CODEC_PREFIX/lib"/*.a
