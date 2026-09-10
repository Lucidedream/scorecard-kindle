#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
CC_BIN=${CC:-/usr/bin/clang}
OUTPUT_DIR="$ROOT_DIR/build-host"

mkdir -p "$OUTPUT_DIR"

"$CC_BIN" -std=c11 -O2 -Wall -Wextra -Werror \
  -I"$ROOT_DIR/third_party/stb" "$SCRIPT_DIR/fontgen.c" -lm -o "$OUTPUT_DIR/fontgen"
"$OUTPUT_DIR/fontgen" "$ROOT_DIR/third_party/public-sans/PublicSans-Medium.ttf" \
  "$ROOT_DIR/src/FontData.h"
