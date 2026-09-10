#pragma once

#include <stdint.h>

#include "core/GolfRound.h"

enum class ScorecardMetric : uint8_t { Par, Score, Putts, In100, Zone, Pen, Count };

struct ScorecardView {
  bool hasPar;
  uint8_t playerSlot;
  uint8_t playerCount;
  char header[64];
  char player[GolfPlayer::NAME_CAPACITY + 8];
  char roundValue[24];
  char cells[static_cast<uint8_t>(ScorecardMetric::Count)][GolfRound::MAX_HOLES][8];
  char out[static_cast<uint8_t>(ScorecardMetric::Count)][8];
  char in[static_cast<uint8_t>(ScorecardMetric::Count)][8];
  char total[static_cast<uint8_t>(ScorecardMetric::Count)][8];
};

struct StatsView {
  bool hasPar;
  uint8_t playerSlot;
  uint8_t playerCount;
  char header[64];
  char player[GolfPlayer::NAME_CAPACITY + 8];
  char score[24];
  char putts[16];
  char onePutts[16];
  char threePutts[16];
  char longGame[16];
  char shortGame[16];
  char putting[16];
  char penalties[40];
  char fairways[16];
  char greens[16];
  char worst[64];
};

ScorecardView scorecardView(const GolfRound& round, uint8_t playerSlot);
StatsView statsView(const GolfRound& round, uint8_t playerSlot);
uint8_t nextEnabledPlayer(const GolfRound& round, uint8_t playerSlot);

