#include <assert.h>
#include <string.h>

#include "MarkSheet.h"
#include "ScoringScreen.h"
#include "core/Course.h"
#include "core/GolfStats.h"
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
  assert(putts.fairway.width > 0 && putts.context.contains(putts.fairway.x, putts.fairway.y));
  const MarkSheetLayout mark = markSheetLayout();
  assert(mark.scrim.y == 0 && mark.sheet.y == mark.scrim.height);
  assert(mark.hazardMinus.width >= 130 && mark.hazardPlus.height >= 130);
  assert(mark.done.y + mark.done.height <= 1448);
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

void testFairwayPillAndToggle() {
  resetRound();
  round.par[0] = 3;
  assert(!scoringView(round, GolfField::Putts).fairwayVisible);
  round.par[0] = 4;
  ScoringView view = scoringView(round, GolfField::Putts);
  assert(view.fairwayVisible && !view.fairwayHit && view.seeded);
  const uint8_t previewPutts = view.values[0];
  const uint8_t previewIn100 = view.values[1];
  assert(toggleGolfFairway(round));
  assert(golfHoleIsLogged(round.players[0].score, 0));
  assert(round.players[0].score.putts[0] == previewPutts);
  assert(round.players[0].score.in100[0] == previewIn100);
  assert(scoringView(round, GolfField::Putts).fairwayHit);
  assert(toggleGolfFairway(round) && !golfFairwayHit(round.players[0].score, 0));
  round.par[0] = 5;
  assert(scoringView(round, GolfField::Putts).fairwayVisible);
  memset(round.par, 0, sizeof(round.par));
  assert(!golfHasPar(round) && scoringView(round, GolfField::Putts).fairwayVisible);
}

void testMarkSheetMutations() {
  resetRound();
  MarkSheetState state = initialMarkSheetState();
  assert(state.field == GolfField::Out100);
  const ScoringView preview = scoringView(round, GolfField::Putts);
  assert(changeMarkPenalty(round, state, GolfPenaltyKind::Hazard, false) == MarkMutationResult::Changed);
  assert(round.players[0].score.putts[0] == preview.values[0]);
  assert(round.players[0].score.in100[0] == preview.values[1]);
  assert(changeMarkPenalty(round, state, GolfPenaltyKind::Hazard, true) == MarkMutationResult::Changed);
  GolfPlayerScore& score = round.players[0].score;
  assert(score.putts[0] == preview.values[0] && score.in100[0] == preview.values[1]);
  assert(score.out100[0] == preview.values[2] + 1);
  assert(golfHazardsForHole(score, 0) == 1 && golfPenaltyStrokesForHole(score, 0) == 1);
  assert(golfPenaltyMarkersForField(score, 0, GolfField::Out100) == 1);
  assert(changeMarkPenalty(round, state, GolfPenaltyKind::Hazard, false) == MarkMutationResult::Changed);
  assert(score.out100[0] == preview.values[2] && score.penaltyCount[0] == 0);

  assert(changeMarkPenalty(round, state, GolfPenaltyKind::Ob, true) == MarkMutationResult::Changed);
  assert(score.out100[0] == preview.values[2] + 1);
  assert(golfPenaltyStrokesForHole(score, 0) == 2);
  selectMarkField(state, GolfField::In100);
  assert(changeMarkPenalty(round, state, GolfPenaltyKind::Hazard, true) == MarkMutationResult::Changed);
  assert(score.in100[0] == preview.values[1] + 1);
  assert(golfPenaltyMarkersForField(score, 0, GolfField::In100, GolfPenaltyKind::Hazard) == 1);

  resetRound();
  state = initialMarkSheetState();
  const ScoringView bunkerPreview = scoringView(round, GolfField::Putts);
  assert(toggleMarkBunker(round));
  assert(golfGreensideBunker(round.players[0].score, 0));
  assert(round.players[0].score.putts[0] == bunkerPreview.values[0]);
  assert(round.players[0].score.in100[0] == bunkerPreview.values[1]);
  assert(toggleMarkBunker(round) && !golfGreensideBunker(round.players[0].score, 0));
}

void testMarkLifoCapAndView() {
  resetRound();
  MarkSheetState state = initialMarkSheetState();
  assert(changeMarkPenalty(round, state, GolfPenaltyKind::Hazard, true) == MarkMutationResult::Changed);
  assert(changeMarkPenalty(round, state, GolfPenaltyKind::Ob, true) == MarkMutationResult::Changed);
  GolfPlayerScore& score = round.players[0].score;
  assert(!golfLatestPenaltyForFieldIs(score, 0, GolfField::Out100, GolfPenaltyKind::Hazard));
  assert(changeMarkPenalty(round, state, GolfPenaltyKind::Hazard, false) == MarkMutationResult::NoChange);
  assert(score.penaltyCount[0] == 2);
  assert(changeMarkPenalty(round, state, GolfPenaltyKind::Ob, false) == MarkMutationResult::Changed);
  assert(changeMarkPenalty(round, state, GolfPenaltyKind::Hazard, false) == MarkMutationResult::Changed);

  for (uint8_t index = 0; index < GolfRound::MAX_PENALTIES_PER_HOLE; ++index) {
    assert(changeMarkPenalty(round, state, GolfPenaltyKind::Hazard, true) == MarkMutationResult::Changed);
  }
  const GolfPlayerScore beforeFull = score;
  assert(changeMarkPenalty(round, state, GolfPenaltyKind::Ob, true) == MarkMutationResult::HoleFull);
  assert(memcmp(&beforeFull, &score, sizeof(score)) == 0 && state.holeFull);
  const MarkSheetView fullView = markSheetView(round, state);
  assert(fullView.holeFull && fullView.hazards == GolfRound::MAX_PENALTIES_PER_HOLE);

  const ScoringView marked = scoringView(round, GolfField::Putts);
  assert(marked.thisHoleValue == static_cast<uint16_t>(score.in100[0] + score.out100[0] + 8));
  assert(strcmp(marked.markLabel, "MARK +8") == 0);
  assert(!marked.fieldMarked[0] && !marked.fieldMarked[1] && marked.fieldMarked[2]);
  golfSetGreensideBunker(score, 0, true);
  assert(scoringView(round, GolfField::Putts).bunkerMarked);
}

void testMarkEventWalkPersists() {
  resetRound();
  MarkSheetState state = initialMarkSheetState();
  const uint16_t previewScore = scoringView(round, GolfField::Putts).thisHoleValue;
  assert(changeMarkPenalty(round, state, GolfPenaltyKind::Hazard, true) == MarkMutationResult::Changed);
  assert(scoringView(round, GolfField::Putts).thisHoleValue == previewScore + 2);
  assert(RoundStore::write(round));
  GolfRound loaded{};
  assert(RoundStore::read(loaded).status == GolfJsonStatus::Ok);
  assert(golfHazardsForHole(loaded.players[0].score, 0) == 1);
  assert(golfPenaltyStrokesForHole(loaded.players[0].score, 0) == 1);
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
  testFairwayPillAndToggle();
  testMarkSheetMutations();
  testMarkLifoCapAndView();
  testMarkEventWalkPersists();
  return 0;
}
