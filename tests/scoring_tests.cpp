#include <assert.h>
#include <string.h>

#include "ScoringScreen.h"
#include "core/Course.h"
#include "store/RoundStore.h"

namespace {

GolfRound round;

void resetRound() {
  round = {};
  assert(applyCourse(round, GOLF_BUILT_IN_COURSES[0], "Blue"));
}

void testLayout() {
  const ScoringLayout putts = scoringLayout(GolfField::Putts);
  assert(putts.header.height == 108);
  assert(putts.holeStrip.height >= 90 && putts.previous.width >= 132 && putts.previous.height >= 132);
  assert(putts.metrics[0].height > putts.metrics[1].height);
  assert(putts.metrics[0].y == putts.context.y + putts.context.height);
  assert(putts.metrics[2].y + putts.metrics[2].height == putts.totals.y);
  assert(putts.footer.y + putts.footer.height == 1448);
  assert(!putts.menu.contains(putts.mark.x, putts.mark.y));
  const ScoringLayout inside = scoringLayout(GolfField::In100);
  assert(inside.metrics[1].height > inside.metrics[0].height);
}

void testSeededAndLoggedView() {
  resetRound();
  round.currentHole = 6;
  const ScoringView seeded = scoringView(round, GolfField::Putts);
  assert(seeded.seeded);
  assert(seeded.values[0] == 2 && seeded.values[1] == 2 && seeded.values[2] == 2);
  assert(seeded.thisHoleValue == 4);
  assert(seeded.thru == 0 && strcmp(seeded.roundValue, "E") == 0);
  assert(!golfHoleIsLogged(round.players[0].score, 6));
  round.players[0].score.putts[6] = 1;
  round.players[0].score.in100[6] = 2;
  round.players[0].score.out100[6] = 3;
  const ScoringView logged = scoringView(round, GolfField::Out100);
  assert(!logged.seeded && logged.values[0] == 1 && logged.values[2] == 3);
  assert(logged.thisHoleValue == 5 && logged.thru == 1);
  assert(strcmp(logged.roundValue, "+1") == 0);
  round.players[0].score.out100[6] = 1;
  assert(strcmp(scoringView(round, GolfField::Out100).roundValue, "-1") == 0);
}

void testParFreeTotals() {
  resetRound();
  memset(round.par, 0, sizeof(round.par));
  round.hasSi = false;
  memset(round.players[0].yards, 0, sizeof(round.players[0].yards));
  ScoringView view = scoringView(round, GolfField::Putts);
  assert(!view.seeded && view.thisHoleValue == 0 && view.context[0] == '\0');
  assert(strcmp(view.hole, "HOLE 1") == 0 && strcmp(view.roundValue, "0") == 0);
  assert(strcmp(view.roundLabel, "ROUND · THRU 0") == 0);
  round.players[0].score.putts[0] = 2;
  round.players[0].score.in100[0] = 2;
  round.players[0].score.out100[0] = 3;
  view = scoringView(round, GolfField::Putts);
  assert(view.thisHoleValue == 5 && strcmp(view.roundValue, "5") == 0);
}

void testForwardCommitRules() {
  resetRound();
  GolfAdvanceResult result = commitAndAdvanceGolfTurn(round);
  assert(result.committed && result.advanced);
  assert(golfHoleIsLogged(round.players[0].score, 0));
  assert(round.currentHole == 1);
  memset(round.par, 0, sizeof(round.par));
  round.players[0].score = {};
  round.currentHole = 0;
  result = commitAndAdvanceGolfTurn(round);
  assert(!result.committed && result.advanced && round.currentHole == 1);
  assert(!golfHoleIsLogged(round.players[0].score, 0));
  resetRound();
  round.players[0].score.in100[0] = 2;
  round.players[0].score.out100[0] = 2;
  result = commitAndAdvanceGolfTurn(round);
  assert(!result.committed && result.advanced);
}

void testTurns() {
  resetRound();
  assert(advanceGolfTurn(round) && round.currentHole == 1 && round.currentPlayer == 0);
  assert(retreatGolfTurn(round) && round.currentHole == 0 && round.currentPlayer == 0);
  golfSetTee(round.players[1], "White");
  golfSetTee(round.players[3], "Red");
  round.currentHole = 17;
  round.currentPlayer = 0;
  assert(advanceGolfTurn(round) && round.currentPlayer == 1 && round.currentHole == 17);
  assert(advanceGolfTurn(round) && round.currentPlayer == 3 && round.currentHole == 17);
  assert(advanceGolfTurn(round) && round.currentPlayer == 0 && round.currentHole == 0);
  assert(retreatGolfTurn(round) && round.currentPlayer == 3 && round.currentHole == 17);
}

void testCounterCommitAndClamp() {
  resetRound();
  assert(changeGolfField(round, GolfField::Putts, true, 20));
  assert(round.players[0].score.putts[0] == 0);
  assert(round.players[0].score.in100[0] == 2);
  assert(round.players[0].score.out100[0] == 2);
  assert(golfHoleIsLogged(round.players[0].score, 0));
  round.players[0].score.putts[0] = 2;
  round.players[0].score.in100[0] = 2;
  assert(changeGolfField(round, GolfField::In100, true, 20));
  assert(round.players[0].score.in100[0] == 0 && round.players[0].score.putts[0] == 0);
  assert(!changeGolfField(round, GolfField::In100, true, 1));
  resetRound();
  assert(changeGolfField(round, GolfField::Putts, false));
  assert(round.players[0].score.putts[0] == 3 && round.players[0].score.in100[0] == 3);
}

void testSyntheticEventWalkPersists() {
  resetRound();
  assert(scoringView(round, GolfField::Putts).seeded);
  assert(changeGolfField(round, GolfField::In100, false));
  assert(round.players[0].score.in100[0] == 3);
  const GolfAdvanceResult advanced = commitAndAdvanceGolfTurn(round);
  assert(!advanced.committed && advanced.advanced && round.currentHole == 1);
  assert(RoundStore::write(round));
  GolfRound loaded{};
  assert(RoundStore::read(loaded).status == GolfJsonStatus::Ok);
  assert(loaded.currentHole == 1 && loaded.players[0].score.in100[0] == 3);
  assert(RoundStore::clear());
}

}  // namespace

int main() {
  testLayout();
  testSeededAndLoggedView();
  testParFreeTotals();
  testForwardCommitRules();
  testTurns();
  testCounterCommitAndClamp();
  testSyntheticEventWalkPersists();
  return 0;
}
