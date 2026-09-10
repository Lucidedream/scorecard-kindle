#pragma once

#include <stdint.h>

#include "HitTester.h"
#include "core/GolfRules.h"

struct ScoringLayout {
  Rect header;
  Rect holeStrip;
  Rect previous;
  Rect next;
  Rect context;
  Rect fairway;
  Rect metrics[3];
  Rect totals;
  Rect thisHole;
  Rect round;
  Rect footer;
  Rect menu;
  Rect mark;
  Rect nextHole;
};

struct ScoringView {
  ScoringLayout layout;
  GolfField focused;
  bool seeded;
  bool hasPar;
  bool fairwayVisible;
  bool fairwayHit;
  bool fieldMarked[3];
  bool bunkerMarked;
  uint8_t values[3];
  uint16_t thisHoleValue;
  uint8_t thru;
  char header[80];
  char hole[32];
  char context[32];
  char roundLabel[32];
  char roundValue[16];
  char markLabel[16];
};

struct GolfAdvanceResult {
  bool committed;
  bool advanced;
};

ScoringLayout scoringLayout(GolfField focused);
ScoringView scoringView(const GolfRound& round, GolfField focused);
bool golfHoleIsLogged(const GolfPlayerScore& score, uint8_t hole);
bool commitGolfPreview(GolfRound& round);
GolfAdvanceResult commitAndAdvanceGolfTurn(GolfRound& round);
bool changeGolfField(GolfRound& round, GolfField field, bool decrement, uint16_t repeatCount = 1);
