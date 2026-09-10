#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
CXX_BIN=${CXX:-/usr/bin/clang++}
OUTPUT_DIR="$SCRIPT_DIR/build-host"

mkdir -p "$OUTPUT_DIR"

"$CXX_BIN" -std=c++20 -stdlib=libc++ -O2 -fno-exceptions -fno-rtti -Wall -Wextra -Werror \
  -I"$SCRIPT_DIR/src" \
  "$SCRIPT_DIR/src/main.cpp" "$SCRIPT_DIR/src/PgmCanvas.cpp" "$SCRIPT_DIR/src/ScoringScreen.cpp" \
  "$SCRIPT_DIR/src/MarkSheet.cpp" "$SCRIPT_DIR/src/ScorecardScreen.cpp" \
  "$SCRIPT_DIR/src/HoleReviewScreen.cpp" "$SCRIPT_DIR/src/SummaryScreen.cpp" \
  "$SCRIPT_DIR/src/SetupScreens.cpp" "$SCRIPT_DIR/src/Keyboard.cpp" \
  "$SCRIPT_DIR/src/TouchInput.cpp" \
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

"$CXX_BIN" -std=c++20 -stdlib=libc++ -O2 -fno-exceptions -fno-rtti -Wall -Wextra -Werror \
  -I"$SCRIPT_DIR/src" \
  "$SCRIPT_DIR/tests/scoring_tests.cpp" "$SCRIPT_DIR/src/ScoringScreen.cpp" "$SCRIPT_DIR/src/MarkSheet.cpp" \
  "$SCRIPT_DIR/src/core/Course.cpp" "$SCRIPT_DIR/src/core/GolfRules.cpp" \
  "$SCRIPT_DIR/src/core/GolfPenalty.cpp" "$SCRIPT_DIR/src/core/GolfStats.cpp" \
  "$SCRIPT_DIR/src/core/GolfValidate.cpp" "$SCRIPT_DIR/src/store/GolfPaths.cpp" \
  "$SCRIPT_DIR/src/store/GolfJson.cpp" "$SCRIPT_DIR/src/store/RoundStore.cpp" \
  -o "$OUTPUT_DIR/scoring_tests"

"$CXX_BIN" -std=c++20 -stdlib=libc++ -O2 -fno-exceptions -fno-rtti -Wall -Wextra -Werror \
  -I"$SCRIPT_DIR/src" \
  "$SCRIPT_DIR/tests/setup_tests.cpp" "$SCRIPT_DIR/src/SetupScreens.cpp" "$SCRIPT_DIR/src/Keyboard.cpp" \
  "$SCRIPT_DIR/src/PgmCanvas.cpp" "$SCRIPT_DIR/src/TouchInput.cpp" "$SCRIPT_DIR/src/core/Course.cpp" \
  "$SCRIPT_DIR/src/core/GolfRules.cpp" "$SCRIPT_DIR/src/core/GolfPenalty.cpp" \
  "$SCRIPT_DIR/src/core/GolfValidate.cpp" "$SCRIPT_DIR/src/store/GolfPaths.cpp" \
  "$SCRIPT_DIR/src/store/GolfJson.cpp" "$SCRIPT_DIR/src/store/RoundStore.cpp" \
  -o "$OUTPUT_DIR/setup_tests"

"$CXX_BIN" -std=c++20 -stdlib=libc++ -O2 -fno-exceptions -fno-rtti -Wall -Wextra -Werror \
  -I"$SCRIPT_DIR/src" \
  "$SCRIPT_DIR/tests/m5_tests.cpp" "$SCRIPT_DIR/src/ScorecardScreen.cpp" \
  "$SCRIPT_DIR/src/HoleReviewScreen.cpp" "$SCRIPT_DIR/src/SummaryScreen.cpp" \
  "$SCRIPT_DIR/src/core/Course.cpp" \
  "$SCRIPT_DIR/src/core/GolfRules.cpp" "$SCRIPT_DIR/src/core/GolfPenalty.cpp" \
  "$SCRIPT_DIR/src/core/GolfStats.cpp" "$SCRIPT_DIR/src/core/GolfValidate.cpp" \
  "$SCRIPT_DIR/src/store/GolfPaths.cpp" "$SCRIPT_DIR/src/store/GolfJson.cpp" \
  "$SCRIPT_DIR/src/store/RoundStore.cpp" "$SCRIPT_DIR/src/store/RoundArchive.cpp" \
  -o "$OUTPUT_DIR/m5_tests"

"$OUTPUT_DIR/m0_tests"
GOLF_TEST_DIR=$(mktemp -d /tmp/scorecard-golf-tests.XXXXXX)
SCORECARD_DIR="$GOLF_TEST_DIR" "$OUTPUT_DIR/golf_tests"
rm -r "$GOLF_TEST_DIR"
SCORING_TEST_DIR=$(mktemp -d /tmp/scorecard-scoring-tests.XXXXXX)
SCORECARD_DIR="$SCORING_TEST_DIR" "$OUTPUT_DIR/scoring_tests"
rm -r "$SCORING_TEST_DIR"
"$OUTPUT_DIR/setup_tests"
M5_TEST_DIR=$(mktemp -d /tmp/scorecard-m5-tests.XXXXXX)
SCORECARD_DIR="$M5_TEST_DIR" "$OUTPUT_DIR/m5_tests"
rm -r "$M5_TEST_DIR"
"$OUTPUT_DIR/scorecard" --render home "$OUTPUT_DIR/home.pgm"
"$OUTPUT_DIR/scorecard" --render keyboard "$OUTPUT_DIR/keyboard.pgm"
"$OUTPUT_DIR/scorecard" --render scoring "$OUTPUT_DIR/scoring.pgm"
"$OUTPUT_DIR/scorecard" --render mark-sheet "$OUTPUT_DIR/mark-sheet.pgm"
"$OUTPUT_DIR/scorecard" --render marked "$OUTPUT_DIR/marked.pgm"
"$OUTPUT_DIR/scorecard" --render scorecard "$OUTPUT_DIR/scorecard.pgm"
"$OUTPUT_DIR/scorecard" --render stats "$OUTPUT_DIR/stats.pgm"
"$OUTPUT_DIR/scorecard" --render hole-review "$OUTPUT_DIR/hole-review.pgm"
"$OUTPUT_DIR/scorecard" --render summary "$OUTPUT_DIR/summary.pgm"
SELFTEST_DIR=$(mktemp -d /tmp/scorecard-selftest.XXXXXX)
SCORECARD_DIR="$SELFTEST_DIR" "$OUTPUT_DIR/scorecard" --selftest
rm -r "$SELFTEST_DIR"
echo "Host tests passed; rendered setup, scoring, scorecard, stats, hole-review, and summary goldens"
