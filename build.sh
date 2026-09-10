#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ZIG_BIN=${ZIG_BIN:-zig}
OUTPUT_DIR="$SCRIPT_DIR/build"

mkdir -p "$OUTPUT_DIR"

if ! command -v "$ZIG_BIN" >/dev/null 2>&1; then
  echo "error: Zig compiler '$ZIG_BIN' not found; install Zig or set ZIG_BIN" >&2
  exit 127
fi

"$ZIG_BIN" c++ \
  -target arm-linux-musleabi \
  -mcpu=cortex_a9 \
  -std=c++20 \
  -Os \
  -static \
  -fno-exceptions \
  -fno-rtti \
  -Wno-nullability-completeness \
  -DCROSSPOINT_GOLF=1 \
  -I"$SCRIPT_DIR/src" \
  -I"$SCRIPT_DIR/src/core" \
  "$SCRIPT_DIR/src/main.cpp" \
  "$SCRIPT_DIR/src/PgmCanvas.cpp" \
  "$SCRIPT_DIR/src/TouchInput.cpp" \
  -o "$OUTPUT_DIR/scorecard"

file "$OUTPUT_DIR/scorecard"
