#pragma once

#include <stdint.h>

#include "core/GolfCareerStats.h"

inline constexpr uint8_t GOLF_CAREER_SECTION_COUNT = 5;
inline constexpr uint8_t GOLF_CAREER_MAX_ROWS = 6;

struct CareerStatsRow {
  const char* label;
  char value[64];
};

struct CareerStatsView {
  char title[40];
  bool empty;
  uint8_t rowCount;
  CareerStatsRow rows[GOLF_CAREER_MAX_ROWS];
};

CareerStatsView careerStatsView(const GolfCareerTally& tally, uint8_t section);
uint8_t stepCareerStatsSection(uint8_t section, bool forward);
