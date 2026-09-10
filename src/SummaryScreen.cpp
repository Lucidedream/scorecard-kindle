#include "SummaryScreen.h"

#include <stdio.h>

#include "ScorecardScreen.h"
#include "core/GolfPenalty.h"
#include "core/GolfStats.h"

SummaryView summaryView(const GolfRound& round, uint8_t playerSlot) {
  SummaryView view{};
  if (playerSlot >= GolfRound::MAX_PLAYERS || !golfPlayerIsEnabled(round.players[playerSlot])) playerSlot = nextEnabledPlayer(round, playerSlot);
  view.playerSlot = playerSlot;
  view.playerCount = golfEnabledPlayerCount(round);
  view.hasPar = golfHasPar(round);
  const GolfPlayer& player = round.players[playerSlot];
  const GolfPlayerScore& score = player.score;
  snprintf(view.header, sizeof(view.header), "ROUND SUMMARY · %s", player.name);
  snprintf(view.player, sizeof(view.player), "%s  >", player.name);
  snprintf(view.score, sizeof(view.score), "%u", golfScore(round, score));
  if (view.hasPar) {
    const int value = golfToPar(round, score);
    if (value == 0) snprintf(view.toPar, sizeof(view.toPar), "E");
    else snprintf(view.toPar, sizeof(view.toPar), "%+d", value);
  }
  snprintf(view.putts, sizeof(view.putts), "%u", golfPuttsTotal(round, score));
  snprintf(view.in100, sizeof(view.in100), "%u", golfIn100Total(round, score));
  snprintf(view.longGame, sizeof(view.longGame), "%u", golfLongTotal(round, score));
  snprintf(view.penalties, sizeof(view.penalties), "%u", golfPenaltyStrokesForRound(score, round.holeCount));
  snprintf(view.fairways, sizeof(view.fairways), "%u/%u", golfFairwaysHit(round, score), golfFairwaysEligible(round, score));
  snprintf(view.greens, sizeof(view.greens), "%u/%u", golfGreensInRegulation(round, score), golfGreensEligible(round, score));
  return view;
}

FinishDestination finishDestination(const bool archiveComplete) {
  return archiveComplete ? FinishDestination::Summary : FinishDestination::ArchiveError;
}
