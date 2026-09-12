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
  -I"$SCRIPT_DIR/src" \
  -I"$SCRIPT_DIR/src/core" \
  "$SCRIPT_DIR/src/main.cpp" \
  "$SCRIPT_DIR/src/Battery.cpp" \
  "$SCRIPT_DIR/src/CareerStatsScreen.cpp" \
  "$SCRIPT_DIR/src/History.cpp" \
  "$SCRIPT_DIR/src/PgmCanvas.cpp" \
  "$SCRIPT_DIR/src/ScoringScreen.cpp" \
  "$SCRIPT_DIR/src/MarkSheet.cpp" \
  "$SCRIPT_DIR/src/ScorecardScreen.cpp" \
  "$SCRIPT_DIR/src/HoleReviewScreen.cpp" \
  "$SCRIPT_DIR/src/SummaryScreen.cpp" \
  "$SCRIPT_DIR/src/SetupScreens.cpp" \
  "$SCRIPT_DIR/src/Keyboard.cpp" \
  "$SCRIPT_DIR/src/TouchInput.cpp" \
  "$SCRIPT_DIR/src/core/Course.cpp" \
  "$SCRIPT_DIR/src/core/GolfRules.cpp" \
  "$SCRIPT_DIR/src/core/GolfPenalty.cpp" \
  "$SCRIPT_DIR/src/core/GolfStats.cpp" \
  "$SCRIPT_DIR/src/core/GolfCareerStats.cpp" \
  "$SCRIPT_DIR/src/core/GolfValidate.cpp" \
  "$SCRIPT_DIR/src/store/GolfPaths.cpp" \
  "$SCRIPT_DIR/src/store/GolfJson.cpp" \
  "$SCRIPT_DIR/src/store/RoundStore.cpp" \
  "$SCRIPT_DIR/src/store/RoundArchive.cpp" \
  -o "$OUTPUT_DIR/scorecard"

file "$OUTPUT_DIR/scorecard"
