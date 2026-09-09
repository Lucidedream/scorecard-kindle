#pragma once

#include <cstdint>

#include "GolfRound.h"

enum class GolfField : uint8_t { Putts = 0, In100 = 1, Out100 = 2 };

struct GolfMutationResult {
  bool changed;
  bool carriedIn100;
  bool loweredPutts;
};

GolfMutationResult incrementGolfCounter(GolfPlayerScore& score, uint8_t hole, GolfField field);
GolfMutationResult decrementGolfCounter(GolfPlayerScore& score, uint8_t hole, GolfField field);
GolfField nextGolfField(GolfField field);
bool seedGolfHoleAtPar(GolfPlayerScore& score, uint8_t hole, uint8_t par);

uint8_t golfFirstEnabledPlayer(const GolfRound& round);
uint8_t golfNextEnabledPlayer(const GolfRound& round, uint8_t player);
uint8_t golfPreviousEnabledPlayer(const GolfRound& round, uint8_t player);
bool golfIsFinalCommit(const GolfRound& round);
bool advanceGolfTurn(GolfRound& round);
bool retreatGolfTurn(GolfRound& round);
