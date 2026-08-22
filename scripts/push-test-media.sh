#!/bin/bash
# Generates real multi-format audio and copies it into the example app's cache
# so the example UI can transcode genuine files during manual QA.
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PACKAGE=com.transcoderexample
OUT="${MEDIA_OUT:-$REPO_ROOT/.media}"
SERIAL=${ANDROID_SERIAL:-}

mkdir -p "$OUT"
cd "$OUT"

TONE="sine=frequency=220:duration=8"
ffmpeg -v error -f lavfi -i "sine=frequency=220:duration=8" \
  -f lavfi -i "sine=frequency=277:duration=8" \
  -f lavfi -i "sine=frequency=330:duration=8" \
  -filter_complex "amix=inputs=3:duration=longest,aformat=sample_rates=48000:channel_layouts=stereo,volume=0.6" \
  -c:a pcm_s16le real-chord.wav -y

ffmpeg -v error -i real-chord.wav -c:a libmp3lame -b:a 192k real-chord.mp3 -y
ffmpeg -v error -i real-chord.wav -c:a aac -b:a 160k real-chord.m4a -y
ffmpeg -v error -i real-chord.wav -c:a libopus -b:a 96k real-chord.opus -y
ffmpeg -v error -i real-chord.wav -c:a flac real-chord.flac -y
ffmpeg -v error -i real-chord.wav -c:a alac real-chord-alac.m4a -y
ffmpeg -v error -i real-chord.wav -c:a pcm_s24le real-chord-24bit.wav -y
ffmpeg -v error -i real-chord.wav -metadata title="Chord Test" -metadata artist="react-native-transcoder" \
  -c:a aac -b:a 128k real-tagged.m4a -y

ADB=(adb)
if [ -n "$SERIAL" ]; then ADB=(adb -s "$SERIAL"); fi

for FILE in real-*; do
  "${ADB[@]}" push "$FILE" /data/local/tmp/ > /dev/null
  "${ADB[@]}" shell "run-as $PACKAGE sh -c 'cat /data/local/tmp/$FILE > cache/$FILE'"
  echo "pushed $FILE"
done

echo "==> files are in /data/data/$PACKAGE/cache — paste a path into the example app's 'file' source"
