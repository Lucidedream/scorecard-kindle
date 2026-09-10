#include "HoleReviewScreen.h"

#include <stdio.h>

#include "core/GolfPenalty.h"
#include "core/GolfStats.h"

uint8_t wrapReviewHole(const GolfRound& round, const uint8_t hole, const int direction) {
  const uint8_t count = round.holeCount == 0 || round.holeCount > GolfRound::MAX_HOLES ? GolfRound::MAX_HOLES
                                                                                       : round.holeCount;
  if (direction < 0) return hole == 0 || hole >= count ? static_cast<uint8_t>(count - 1) : static_cast<uint8_t>(hole - 1);
  return static_cast<uint8_t>(hole + 1 >= count ? 0 : hole + 1);
}

HoleReviewView holeReviewView(const GolfRound& round, uint8_t playerSlot, uint8_t hole) {
  HoleReviewView view{};
  if (playerSlot >= GolfRound::MAX_PLAYERS || !golfPlayerIsEnabled(round.players[playerSlot])) playerSlot = 0;
  if (hole >= round.holeCount || hole >= GolfRound::MAX_HOLES) hole = 0;
  view.hole = hole;
  view.hasPar = golfHasPar(round);
  const GolfPlayer& player = round.players[playerSlot];
  const GolfPlayerScore& score = player.score;
  view.entered = static_cast<uint16_t>(score.in100[hole]) + score.out100[hole] != 0;
  if (view.hasPar) snprintf(view.strip, sizeof(view.strip), "<  HOLE %u · PAR %u  >", hole + 1, round.par[hole]);
  else snprintf(view.strip, sizeof(view.strip), "<  HOLE %u  >", hole + 1);
  if (player.yards[hole] != 0 && round.hasSi)
    snprintf(view.context, sizeof(view.context), "%s · %u YD · SI %u", player.tee, player.yards[hole], round.si[hole]);
  else if (player.yards[hole] != 0) snprintf(view.context, sizeof(view.context), "%s · %u YD", player.tee, player.yards[hole]);
  else snprintf(view.context, sizeof(view.context), "%s", player.tee);
  if (!view.entered) snprintf(view.hero, sizeof(view.hero), "-");
  else if (view.hasPar) {
    const int toPar = static_cast<int>(golfHoleScore(round, score, hole)) - round.par[hole];
    if (toPar == 0) snprintf(view.hero, sizeof(view.hero), "%u   E", golfHoleScore(round, score, hole));
    else snprintf(view.hero, sizeof(view.hero), "%u   %+d", golfHoleScore(round, score, hole), toPar);
  } else snprintf(view.hero, sizeof(view.hero), "%u", golfHoleScore(round, score, hole));
  if (view.entered) {
    snprintf(view.putts, sizeof(view.putts), "%u", score.putts[hole]);
    snprintf(view.in100, sizeof(view.in100), "%u", score.in100[hole]);
    snprintf(view.zone, sizeof(view.zone), "%u", score.out100[hole]);
  } else {
    snprintf(view.putts, sizeof(view.putts), "-");
    snprintf(view.in100, sizeof(view.in100), "-");
    snprintf(view.zone, sizeof(view.zone), "-");
  }
  const uint16_t strokes = golfPenaltyStrokesForHole(score, hole);
  view.hasPenalty = strokes != 0;
  if (view.hasPenalty) snprintf(view.penalty, sizeof(view.penalty), "PENALTY +%u   HZD %u · OB %u", strokes,
                                golfHazardsForHole(score, hole), golfObsForHole(score, hole));
  view.fairway = golfFairwayHit(score, hole);
  view.bunker = golfGreensideBunker(score, hole);
  if (view.fairway && view.bunker) snprintf(view.marks, sizeof(view.marks), "FAIRWAY · BUNKER");
  else if (view.fairway) snprintf(view.marks, sizeof(view.marks), "FAIRWAY");
  else if (view.bunker) snprintf(view.marks, sizeof(view.marks), "BUNKER");
  return view;
}
