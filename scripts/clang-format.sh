#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."

FILES=$(find packages/react-native-transcoder/cpp packages/react-native-transcoder/android/src \
  -type f \( -name '*.cpp' -o -name '*.hpp' -o -name '*.h' -o -name '*.mm' \) 2>/dev/null || true)

if [ -z "$FILES" ]; then
  echo "no C++ files found"
  exit 0
fi

if [ "${1:-}" == "--check" ]; then
  echo "$FILES" | xargs clang-format --dry-run --Werror
else
  echo "$FILES" | xargs clang-format -i
fi
