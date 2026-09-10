#pragma once

#include <stdint.h>

#include "GolfRound.h"

// Career-stats tally (CONTRACTS-V2 §33.4). Accumulated across one player's
// 18-hole rounds by golfFoldCareerRound() and rendered by
// GolfCareerStatsActivity. Zero-initialise with `GolfCareerTally tally{};`.
struct GolfCareerTally {
  uint16_t rounds;     // 18-hole rounds folded
  uint16_t parRounds;  // of those, rounds whose every hole carried a known par
  uint32_t strokes;    // gross over all folded rounds (entered holes)
  uint32_t parTotal;   // played par over all folded rounds
  int32_t toParTotal;  // gross-minus-par over the parRounds rounds

  // Score distribution, bucketed by golfScoreVsPar on entered par-3..6 holes:
  // 0 eagle+ (<= -2), 1 birdie (-1), 2 par (0), 3 bogey (+1), 4 double (+2),
  // 5 triple+ (>= +3).
  uint16_t dist[6];

  uint16_t parStrokes[3];  // stroke sum on entered par-3 / par-4 / par-5 holes
  uint16_t parHoles[3];    // count of those holes

  uint16_t scrambles;
  uint16_t scrambleChances;
  uint16_t sandSaves;
  uint16_t sandSaveChances;
  uint16_t puttsOnGir;      // putts on GIR holes
  uint16_t girHoles;        // count of GIR holes
  uint16_t threePuttHoles;  // entered holes with 3+ putts
  uint16_t holesPlayed;     // entered holes across all folded rounds

  uint16_t lowestRound;         // lowest complete-18 gross; 0 = none yet (par-free included)
  char lowestCourse[40];        // course of the lowestRound
  uint16_t lowestNine;          // lowest fully-entered front or back nine gross; 0 = none
  uint16_t fewestPutts;         // fewest putts in a complete-18 round; 0 = none
  uint8_t mostPars;             // most holes exactly at par in one round
  uint8_t longestBogeyFreeRun;  // longest run of consecutive entered holes at par or better
};

// Fold one round's `slot` score into `tally`. No-op unless round.holeCount == 18
// and the slot is enabled. Pure — no I/O.
void golfFoldCareerRound(const GolfRound& round, uint8_t slot, GolfCareerTally& tally);
