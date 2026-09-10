#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
CXX_BIN=${CXX:-/usr/bin/clang++}
OUTPUT_DIR="$SCRIPT_DIR/build-host"

mkdir -p "$OUTPUT_DIR"

"$CXX_BIN" -std=c++20 -stdlib=libc++ -O2 -fno-exceptions -fno-rtti -Wall -Wextra -Werror \
  -I"$SCRIPT_DIR/src" \
  "$SCRIPT_DIR/src/main.cpp" "$SCRIPT_DIR/src/PgmCanvas.cpp" "$SCRIPT_DIR/src/TouchInput.cpp" \
  "$SCRIPT_DIR/src/core/Course.cpp" "$SCRIPT_DIR/src/core/GolfRules.cpp" \
  "$SCRIPT_DIR/src/core/GolfPenalty.cpp" "$SCRIPT_DIR/src/core/GolfStats.cpp" \
  "$SCRIPT_DIR/src/core/GolfCareerStats.cpp" "$SCRIPT_DIR/src/core/GolfValidate.cpp" \
  "$SCRIPT_DIR/src/store/GolfPaths.cpp" "$SCRIPT_DIR/src/store/GolfJson.cpp" \
  "$SCRIPT_DIR/src/store/RoundStore.cpp" "$SCRIPT_DIR/src/store/RoundArchive.cpp" \
  -o "$OUTPUT_DIR/scorecard"

"$CXX_BIN" -std=c++20 -stdlib=libc++ -O2 -fno-exceptions -fno-rtti -Wall -Wextra -Werror \
  -I"$SCRIPT_DIR/src" \
  "$SCRIPT_DIR/tests/m0_tests.cpp" "$SCRIPT_DIR/src/PgmCanvas.cpp" "$SCRIPT_DIR/src/TouchInput.cpp" \
  -o "$OUTPUT_DIR/m0_tests"

"$CXX_BIN" -std=c++20 -stdlib=libc++ -O2 -fno-exceptions -fno-rtti -Wall -Wextra -Werror \
  -I"$SCRIPT_DIR/src" \
  "$SCRIPT_DIR/tests/golf_tests.cpp" \
  "$SCRIPT_DIR/src/core/Course.cpp" "$SCRIPT_DIR/src/core/GolfRules.cpp" \
  "$SCRIPT_DIR/src/core/GolfPenalty.cpp" "$SCRIPT_DIR/src/core/GolfStats.cpp" \
  "$SCRIPT_DIR/src/core/GolfCareerStats.cpp" "$SCRIPT_DIR/src/core/GolfValidate.cpp" \
  "$SCRIPT_DIR/src/store/GolfPaths.cpp" "$SCRIPT_DIR/src/store/GolfJson.cpp" \
  "$SCRIPT_DIR/src/store/RoundStore.cpp" "$SCRIPT_DIR/src/store/RoundArchive.cpp" \
  -o "$OUTPUT_DIR/golf_tests"

"$OUTPUT_DIR/m0_tests"
GOLF_TEST_DIR=$(mktemp -d /tmp/scorecard-golf-tests.XXXXXX)
SCORECARD_DIR="$GOLF_TEST_DIR" "$OUTPUT_DIR/golf_tests"
rm -r "$GOLF_TEST_DIR"
"$OUTPUT_DIR/scorecard" --render "$OUTPUT_DIR/demo.pgm"
SELFTEST_DIR=$(mktemp -d /tmp/scorecard-selftest.XXXXXX)
SCORECARD_DIR="$SELFTEST_DIR" "$OUTPUT_DIR/scorecard" --selftest
rm -r "$SELFTEST_DIR"
echo "Host tests passed; rendered $OUTPUT_DIR/demo.pgm"
