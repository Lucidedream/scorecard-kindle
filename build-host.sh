#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
CXX_BIN=${CXX:-/usr/bin/clang++}
OUTPUT_DIR="$SCRIPT_DIR/build-host"

mkdir -p "$OUTPUT_DIR"

"$CXX_BIN" -std=c++20 -stdlib=libc++ -O2 -fno-exceptions -fno-rtti -Wall -Wextra -Werror \
  -I"$SCRIPT_DIR/src" \
  "$SCRIPT_DIR/src/main.cpp" "$SCRIPT_DIR/src/PgmCanvas.cpp" "$SCRIPT_DIR/src/TouchInput.cpp" \
  -o "$OUTPUT_DIR/scorecard"

"$CXX_BIN" -std=c++20 -stdlib=libc++ -O2 -fno-exceptions -fno-rtti -Wall -Wextra -Werror \
  -I"$SCRIPT_DIR/src" \
  "$SCRIPT_DIR/tests/m0_tests.cpp" "$SCRIPT_DIR/src/PgmCanvas.cpp" "$SCRIPT_DIR/src/TouchInput.cpp" \
  -o "$OUTPUT_DIR/m0_tests"

"$OUTPUT_DIR/m0_tests"
"$OUTPUT_DIR/scorecard" --render "$OUTPUT_DIR/demo.pgm"
echo "Host tests passed; rendered $OUTPUT_DIR/demo.pgm"
