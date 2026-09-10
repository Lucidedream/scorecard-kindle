#pragma once

#include <stdint.h>

// Kindle Voyage panels expose 16 stable gray levels. Keep every deliberate
// UI ink on that hardware palette; glyph antialiasing blends between levels.
inline constexpr uint8_t INK_DIM = 119;       // 7 * 17: secondary labels
inline constexpr uint8_t INK_GHOST = 153;     // 9 * 17: uncommitted preview values
inline constexpr uint8_t INK_HAIRLINE = 204;  // 12 * 17: quiet structural rules

