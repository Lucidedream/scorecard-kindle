#pragma once

#include <stdint.h>

#include "core/GolfRound.h"

struct HoleReviewView {
  uint8_t hole;
  bool hasPar;
  bool entered;
  bool hasPenalty;
  bool fairway;
  bool bunker;
  char strip[40];
  char context[64];
  char hero[32];
  char putts[8];
  char in100[8];
  char zone[8];
  char penalty[64];
  char marks[32];
};

HoleReviewView holeReviewView(const GolfRound& round, uint8_t playerSlot, uint8_t hole);
uint8_t wrapReviewHole(const GolfRound& round, uint8_t hole, int direction);

