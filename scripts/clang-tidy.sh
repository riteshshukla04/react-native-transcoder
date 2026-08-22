#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."

BUILD_DIR=${1:-build/host}
if [ ! -f "$BUILD_DIR/compile_commands.json" ]; then
  echo "no compile_commands.json in $BUILD_DIR — run: cmake --preset host" >&2
  exit 1
fi

find packages/react-native-transcoder/cpp -name '*.cpp' -print0 \
  | xargs -0 clang-tidy -p "$BUILD_DIR"
