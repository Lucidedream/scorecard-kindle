#include "ScorecardScreen.h"

#include <stdio.h>
#include <string.h>

#include "core/GolfPenalty.h"
#include "core/GolfStats.h"

namespace {

constexpr uint8_t metric(ScorecardMetric value) { return static_cast<uint8_t>(value); }

bool entered(const GolfPlayerScore& score, const uint8_t hole) {
  return static_cast<uint16_t>(score.in100[hole]) + score.out100[hole] != 0;
}

void number(char* output, const size_t capacity, const unsigned value) {
  snprintf(output, capacity, "%u", value);
}

void signedValue(char* output, const size_t capacity, const int value) {
  if (value == 0) snprintf(output, capacity, "E");
  else snprintf(output, capacity, "%+d", value);
}

uint16_t rangeTotal(const GolfRound& round, const GolfPlayerScore& score, const ScorecardMetric row,
                    const uint8_t first, const uint8_t last) {
  uint16_t result = 0;
  const uint8_t limit = last < round.holeCount ? last : round.holeCount;
  for (uint8_t hole = first; hole < limit; ++hole) {
    if (!entered(score, hole)) continue;
    switch (row) {
      case ScorecardMetric::Par: result += round.par[hole]; break;
      case ScorecardMetric::Score: result += golfHoleScore(round, score, hole); break;
      case ScorecardMetric::Putts: result += score.putts[hole]; break;
      case ScorecardMetric::In100: result += score.in100[hole]; break;
      case ScorecardMetric::Zone: result += score.out100[hole]; break;
      case ScorecardMetric::Pen: result += golfPenaltyStrokesForHole(score, hole); break;
      case ScorecardMetric::Count: break;
    }
  }
  return result;
}

void rangeString(char* output, const size_t capacity, const GolfRound& round, const GolfPlayerScore& score,
                 const ScorecardMetric row, const uint8_t first, const uint8_t last) {
  bool any = false;
  const uint8_t limit = last < round.holeCount ? last : round.holeCount;
  for (uint8_t hole = first; hole < limit; ++hole) any = any || entered(score, hole);
  if (!any) snprintf(output, capacity, "-");
  else {
    const uint16_t value = rangeTotal(round, score, row, first, last);
    if (row == ScorecardMetric::Pen && value == 0) snprintf(output, capacity, "-");
    else number(output, capacity, value);
  }
}

uint8_t usablePlayer(const GolfRound& round, uint8_t slot) {
  if (slot < GolfRound::MAX_PLAYERS && golfPlayerIsEnabled(round.players[slot])) return slot;
  for (uint8_t candidate = 0; candidate < GolfRound::MAX_PLAYERS; ++candidate) {
    if (golfPlayerIsEnabled(round.players[candidate])) return candidate;
  }
  return 0;
}

}  // namespace

uint8_t nextEnabledPlayer(const GolfRound& round, const uint8_t playerSlot) {
  for (uint8_t step = 1; step <= GolfRound::MAX_PLAYERS; ++step) {
    const uint8_t candidate = static_cast<uint8_t>((playerSlot + step) % GolfRound::MAX_PLAYERS);
    if (golfPlayerIsEnabled(round.players[candidate])) return candidate;
  }
  return playerSlot;
}

ScorecardView scorecardView(const GolfRound& round, uint8_t playerSlot) {
  ScorecardView view{};
  playerSlot = usablePlayer(round, playerSlot);
  view.playerSlot = playerSlot;
  view.playerCount = golfEnabledPlayerCount(round);
  view.hasPar = golfHasPar(round);
  snprintf(view.header, sizeof(view.header), "SCORECARD · %s", round.players[playerSlot].name);
  snprintf(view.player, sizeof(view.player), "%s  >", round.players[playerSlot].name);
  const GolfPlayerScore& score = round.players[playerSlot].score;
  if (view.hasPar) {
    char toPar[12];
    signedValue(toPar, sizeof(toPar), golfToPar(round, score));
    snprintf(view.roundValue, sizeof(view.roundValue), "%u · %s", golfScore(round, score), toPar);
  } else {
    snprintf(view.roundValue, sizeof(view.roundValue), "%u", golfScore(round, score));
  }

  for (uint8_t hole = 0; hole < GolfRound::MAX_HOLES; ++hole) {
    if (view.hasPar && hole < round.holeCount) number(view.cells[metric(ScorecardMetric::Par)][hole], 8, round.par[hole]);
    else snprintf(view.cells[metric(ScorecardMetric::Par)][hole], 8, "-");
    if (!entered(score, hole) || hole >= round.holeCount) {
      for (uint8_t row = metric(ScorecardMetric::Score); row < metric(ScorecardMetric::Count); ++row)
        snprintf(view.cells[row][hole], 8, "-");
      continue;
    }
    number(view.cells[metric(ScorecardMetric::Score)][hole], 8, golfHoleScore(round, score, hole));
    number(view.cells[metric(ScorecardMetric::Putts)][hole], 8, score.putts[hole]);
    number(view.cells[metric(ScorecardMetric::In100)][hole], 8, score.in100[hole]);
    number(view.cells[metric(ScorecardMetric::Zone)][hole], 8, score.out100[hole]);
    const uint16_t penalties = golfPenaltyStrokesForHole(score, hole);
    if (penalties == 0) snprintf(view.cells[metric(ScorecardMetric::Pen)][hole], 8, "-");
    else number(view.cells[metric(ScorecardMetric::Pen)][hole], 8, penalties);
  }
  for (uint8_t row = 0; row < metric(ScorecardMetric::Count); ++row) {
    rangeString(view.out[row], 8, round, score, static_cast<ScorecardMetric>(row), 0, 9);
    rangeString(view.in[row], 8, round, score, static_cast<ScorecardMetric>(row), 9, 18);
    rangeString(view.total[row], 8, round, score, static_cast<ScorecardMetric>(row), 0, 18);
  }
  return view;
}

StatsView statsView(const GolfRound& round, uint8_t playerSlot) {
  StatsView view{};
  playerSlot = usablePlayer(round, playerSlot);
  view.playerSlot = playerSlot;
  view.playerCount = golfEnabledPlayerCount(round);
  view.hasPar = golfHasPar(round);
  snprintf(view.header, sizeof(view.header), "STATS · %s", round.players[playerSlot].name);
  snprintf(view.player, sizeof(view.player), "%s  >", round.players[playerSlot].name);
  const GolfPlayerScore& score = round.players[playerSlot].score;
  if (view.hasPar) {
    char toPar[12];
    signedValue(toPar, sizeof(toPar), golfToPar(round, score));
    snprintf(view.score, sizeof(view.score), "%u · %s", golfScore(round, score), toPar);
  } else snprintf(view.score, sizeof(view.score), "%u", golfScore(round, score));
  number(view.putts, sizeof(view.putts), golfPuttsTotal(round, score));
  number(view.onePutts, sizeof(view.onePutts), golfOnePutts(round, score));
  number(view.threePutts, sizeof(view.threePutts), golfThreePutts(round, score));
  number(view.longGame, sizeof(view.longGame), golfLongTotal(round, score));
  number(view.shortGame, sizeof(view.shortGame), golfShortTotal(round, score));
  number(view.putting, sizeof(view.putting), golfPuttsTotal(round, score));
  snprintf(view.penalties, sizeof(view.penalties), "%u  (HZD %u · OB %u)",
           golfPenaltyStrokesForRound(score, round.holeCount), golfHazardsForRound(score, round.holeCount),
           golfObsForRound(score, round.holeCount));
  snprintf(view.fairways, sizeof(view.fairways), "%u/%u", golfFairwaysHit(round, score),
           golfFairwaysEligible(round, score));
  snprintf(view.greens, sizeof(view.greens), "%u/%u", golfGreensInRegulation(round, score),
           golfGreensEligible(round, score));
  if (view.hasPar) {
    GolfWorstHole holes[3]{};
    const uint8_t count = golfWorstHoles(round, score, holes, 3);
    size_t used = 0;
    for (uint8_t index = 0; index < count; ++index) {
      const int written = snprintf(view.worst + used, sizeof(view.worst) - used, "%sH%u %+d",
                                   index == 0 ? "" : " · ", holes[index].hole + 1, holes[index].toPar);
      if (written < 0 || static_cast<size_t>(written) >= sizeof(view.worst) - used) break;
      used += static_cast<size_t>(written);
    }
    if (count == 0) snprintf(view.worst, sizeof(view.worst), "-");
  }
  return view;
}
