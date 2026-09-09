#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ZIG_BIN=${ZIG_BIN:-zig}
OUTPUT_DIR="$SCRIPT_DIR/build"

mkdir -p "$OUTPUT_DIR"

"$ZIG_BIN" c++ \
  -target arm-linux-musleabi \
  -mcpu=cortex_a9 \
  -std=c++20 \
  -Os \
  -static \
  -Wno-nullability-completeness \
  -DCROSSPOINT_GOLF=1 \
  -I"$SCRIPT_DIR/src" \
  -I"$SCRIPT_DIR/src/core" \
  "$SCRIPT_DIR/src/main.cpp" \
  "$SCRIPT_DIR/src/PgmCanvas.cpp" \
  "$SCRIPT_DIR/src/TouchInput.cpp" \
  "$SCRIPT_DIR/src/core/GolfRules.cpp" \
  -o "$OUTPUT_DIR/scorecard"

file "$OUTPUT_DIR/scorecard"
