#include "GolfValidate.h"


#include <string.h>

#include "GolfPenalty.h"
#include "GolfRules.h"

namespace {

uint8_t packedAt(const GolfPlayerScore& score, const uint8_t hole, const uint8_t index) {
  const uint8_t packed = score.penaltyEvents[hole][index / 2];
  return index % 2 == 0 ? static_cast<uint8_t>(packed & 0x0f) : static_cast<uint8_t>(packed >> 4);
}

void storeAt(GolfPlayerScore& score, const uint8_t hole, const uint8_t index, const uint8_t packed) {
  uint8_t& target = score.penaltyEvents[hole][index / 2];
  const uint8_t shift = index % 2 == 0 ? 0 : 4;
  target = static_cast<uint8_t>((target & ~(0x0f << shift)) | ((packed & 0x0f) << shift));
}

uint8_t fieldShotCount(const GolfPlayerScore& score, const uint8_t hole, const GolfField field) {
  switch (field) {
    case GolfField::Putts:
      return score.putts[hole];
    case GolfField::In100:
      return score.in100[hole];
    case GolfField::Out100:
      return score.out100[hole];
  }
  return 0;
}

// The tee name is NUL-terminated within its buffer. An empty string is the
// "did not play" sentinel; deep tee-name validation happens at decode
// (golfTeeStringValid), mirroring how player names are checked there, not here.
bool validTee(const char* tee) { return memchr(tee, '\0', GOLF_TEE_CAPACITY) != nullptr; }

}  // namespace

GolfPlayerValidationResult validateGolfPlayerScore(GolfPlayerScore& score, const uint8_t holeCount) {
  GolfPlayerValidationResult result{};
  const uint8_t holes = holeCount < GolfRound::MAX_HOLES ? holeCount : GolfRound::MAX_HOLES;
  for (uint8_t hole = 0; hole < holes; ++hole) {
    const uint8_t oldPutts = score.putts[hole];
    const uint8_t oldIn100 = score.in100[hole];
    if (score.in100[hole] == 0 && score.out100[hole] == 0) {
      score.putts[hole] = 0;
    } else if (score.putts[hole] > score.in100[hole]) {
      score.putts[hole] = score.in100[hole];
    }
    if (score.putts[hole] != oldPutts) result.puttsRepaired |= 1UL << hole;
    if (score.in100[hole] != oldIn100) result.in100Repaired |= 1UL << hole;

    const uint8_t originalCount = score.penaltyCount[hole];
    const uint8_t readableCount =
        originalCount < GolfRound::MAX_PENALTIES_PER_HOLE ? originalCount : GolfRound::MAX_PENALTIES_PER_HOLE;
    if (originalCount > GolfRound::MAX_PENALTIES_PER_HOLE) result.penaltyCountRepaired |= 1UL << hole;

    uint8_t fieldMarkers[3]{};
    uint8_t repairedCount = 0;
    for (uint8_t index = 0; index < readableCount; ++index) {
      GolfPenaltyEvent event{};
      const uint8_t packed = packedAt(score, hole, index);
      if (!golfUnpackPenaltyEvent(packed, event)) {
        result.penaltyEventRepaired |= 1UL << hole;
        continue;
      }
      const uint8_t field = static_cast<uint8_t>(event.field);
      if (fieldMarkers[field] >= fieldShotCount(score, hole, event.field)) {
        result.penaltyMarkerRepaired |= 1UL << hole;
        continue;
      }
      ++fieldMarkers[field];
      storeAt(score, hole, repairedCount++, packed);
    }
    if (repairedCount != readableCount) result.penaltyCountRepaired |= 1UL << hole;
    score.penaltyCount[hole] = repairedCount;
    while (repairedCount < GolfRound::MAX_PENALTIES_PER_HOLE) storeAt(score, hole, repairedCount++, 0);
  }
  return result;
}

GolfValidationResult validateGolfRound(GolfRound& round) {
  GolfValidationResult result{};
  result.valid = round.holeCount == 18;
  uint8_t enabledPlayers = 0;
  for (const GolfPlayer& player : round.players) {
    if (!validTee(player.tee)) {
      result.valid = false;
      return result;
    }
    if (golfPlayerIsEnabled(player)) ++enabledPlayers;
  }
  if (!result.valid) return result;

  if (enabledPlayers == 0) {
    golfSetTee(round.players[0], "Blue");
    result.firstPlayerEnabled = true;
  }

  if (round.currentHole >= round.holeCount) {
    round.currentHole = 0;
    result.currentHoleReset = true;
  }
  if (round.currentPlayer >= GolfRound::MAX_PLAYERS || !golfPlayerIsEnabled(round.players[round.currentPlayer])) {
    round.currentPlayer = golfFirstEnabledPlayer(round);
    result.currentPlayerReset = true;
  }

  for (uint8_t player = 0; player < GolfRound::MAX_PLAYERS; ++player) {
    result.players[player] = validateGolfPlayerScore(round.players[player].score, round.holeCount);
  }
  return result;
}
