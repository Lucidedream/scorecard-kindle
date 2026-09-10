#pragma once

#include <stdint.h>

#include "GolfRound.h"

struct GolfPlayerValidationResult {
  uint32_t puttsRepaired;
  uint32_t in100Repaired;
  uint32_t penaltyCountRepaired;
  uint32_t penaltyEventRepaired;
  uint32_t penaltyMarkerRepaired;

  bool repaired() const {
    return puttsRepaired != 0 || in100Repaired != 0 || penaltyCountRepaired != 0 || penaltyEventRepaired != 0 ||
           penaltyMarkerRepaired != 0;
  }
  bool holePuttsRepaired(uint8_t hole) const { return (puttsRepaired & (1UL << hole)) != 0; }
  bool holeIn100Repaired(uint8_t hole) const { return (in100Repaired & (1UL << hole)) != 0; }
  bool holePenaltyCountRepaired(uint8_t hole) const { return (penaltyCountRepaired & (1UL << hole)) != 0; }
  bool holePenaltyEventRepaired(uint8_t hole) const { return (penaltyEventRepaired & (1UL << hole)) != 0; }
  bool holePenaltyMarkerRepaired(uint8_t hole) const { return (penaltyMarkerRepaired & (1UL << hole)) != 0; }
};

struct GolfValidationResult {
  bool valid;
  bool currentHoleReset;
  bool currentPlayerReset;
  bool firstPlayerEnabled;
  GolfPlayerValidationResult players[GolfRound::MAX_PLAYERS];

  bool repaired() const {
    if (currentHoleReset || currentPlayerReset || firstPlayerEnabled) return true;
    for (const GolfPlayerValidationResult& player : players) {
      if (player.repaired()) return true;
    }
    return false;
  }
};

GolfPlayerValidationResult validateGolfPlayerScore(GolfPlayerScore& score, uint8_t holeCount);
GolfValidationResult validateGolfRound(GolfRound& round);
