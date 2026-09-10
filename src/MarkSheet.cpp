#include "MarkSheet.h"

#include <stdio.h>

#include "PgmCanvas.h"
#include "ScoringScreen.h"

namespace {

bool currentScore(GolfRound& round, GolfPlayerScore*& score) {
  if (round.currentHole >= round.holeCount || round.currentHole >= GolfRound::MAX_HOLES ||
      round.currentPlayer >= GolfRound::MAX_PLAYERS ||
      !golfPlayerIsEnabled(round.players[round.currentPlayer])) {
    return false;
  }
  score = &round.players[round.currentPlayer].score;
  return true;
}

bool currentScore(const GolfRound& round, const GolfPlayerScore*& score) {
  if (round.currentHole >= round.holeCount || round.currentHole >= GolfRound::MAX_HOLES ||
      round.currentPlayer >= GolfRound::MAX_PLAYERS ||
      !golfPlayerIsEnabled(round.players[round.currentPlayer])) {
    return false;
  }
  score = &round.players[round.currentPlayer].score;
  return true;
}

}  // namespace

MarkSheetState initialMarkSheetState() { return {GolfField::Out100, false}; }

MarkSheetLayout markSheetLayout() {
  MarkSheetLayout layout{};
  constexpr int MARGIN = 24;
  constexpr int SHEET_TOP = 350;
  layout.scrim = {0, 0, PgmCanvas::WIDTH, SHEET_TOP};
  layout.sheet = {MARGIN, SHEET_TOP, PgmCanvas::WIDTH - 2 * MARGIN, PgmCanvas::HEIGHT - SHEET_TOP};
  layout.title = {layout.sheet.x + 28, SHEET_TOP + 20, layout.sheet.width - 56, 96};
  layout.fieldLabel = {layout.sheet.x + 28, SHEET_TOP + 120, layout.sheet.width - 56, 54};
  const int segmentY = SHEET_TOP + 176;
  const int segmentWidth = (layout.sheet.width - 56) / 2;
  layout.out100Segment = {layout.sheet.x + 28, segmentY, segmentWidth, 132};
  layout.in100Segment = {layout.out100Segment.x + segmentWidth, segmentY, segmentWidth, 132};
  layout.bunkerRow = {layout.sheet.x + 28, SHEET_TOP + 330, layout.sheet.width - 56, 150};
  layout.hazardRow = {layout.sheet.x + 28, SHEET_TOP + 492, layout.sheet.width - 56, 150};
  layout.obRow = {layout.sheet.x + 28, SHEET_TOP + 654, layout.sheet.width - 56, 150};
  constexpr int BUTTON = 140;
  layout.hazardMinus = {layout.hazardRow.x + layout.hazardRow.width - 3 * BUTTON, layout.hazardRow.y,
                        BUTTON, layout.hazardRow.height};
  layout.hazardPlus = {layout.hazardRow.x + layout.hazardRow.width - BUTTON, layout.hazardRow.y,
                       BUTTON, layout.hazardRow.height};
  layout.obMinus = {layout.obRow.x + layout.obRow.width - 3 * BUTTON, layout.obRow.y, BUTTON,
                    layout.obRow.height};
  layout.obPlus = {layout.obRow.x + layout.obRow.width - BUTTON, layout.obRow.y, BUTTON,
                   layout.obRow.height};
  layout.status = {layout.sheet.x + 28, SHEET_TOP + 812, layout.sheet.width - 56, 54};
  layout.done = {layout.sheet.x + 28, SHEET_TOP + 886, layout.sheet.width - 56, 164};
  return layout;
}

uint8_t golfPenaltyMarkersForField(const GolfPlayerScore& score, const uint8_t hole,
                                   const GolfField field, const GolfPenaltyKind kind) {
  if (hole >= GolfRound::MAX_HOLES) return 0;
  uint8_t count = 0;
  for (uint8_t index = 0; index < score.penaltyCount[hole] &&
                                  index < GolfRound::MAX_PENALTIES_PER_HOLE;
       ++index) {
    GolfPenaltyEvent event{};
    if (golfPenaltyEventAt(score, hole, index, event) && event.field == field && event.kind == kind) ++count;
  }
  return count;
}

uint8_t golfPenaltyMarkersForField(const GolfPlayerScore& score, const uint8_t hole,
                                   const GolfField field) {
  return static_cast<uint8_t>(golfPenaltyMarkersForField(score, hole, field, GolfPenaltyKind::Hazard) +
                              golfPenaltyMarkersForField(score, hole, field, GolfPenaltyKind::Ob));
}

bool golfLatestPenaltyForFieldIs(const GolfPlayerScore& score, const uint8_t hole,
                                 const GolfField field, const GolfPenaltyKind kind) {
  if (hole >= GolfRound::MAX_HOLES) return false;
  uint8_t index = score.penaltyCount[hole];
  if (index > GolfRound::MAX_PENALTIES_PER_HOLE) index = GolfRound::MAX_PENALTIES_PER_HOLE;
  while (index > 0) {
    GolfPenaltyEvent event{};
    --index;
    if (golfPenaltyEventAt(score, hole, index, event) && event.field == field) return event.kind == kind;
  }
  return false;
}

MarkSheetView markSheetView(const GolfRound& round, const MarkSheetState& state) {
  MarkSheetView view{};
  view.layout = markSheetLayout();
  view.field = state.field;
  view.holeFull = state.holeFull;
  snprintf(view.title, sizeof(view.title), "MARK · HOLE %u", static_cast<unsigned>(round.currentHole + 1));
  const GolfPlayerScore* score = nullptr;
  if (!currentScore(round, score)) return view;
  view.bunker = golfGreensideBunker(*score, round.currentHole);
  view.hazards = golfPenaltyMarkersForField(*score, round.currentHole, state.field,
                                           GolfPenaltyKind::Hazard);
  view.obs = golfPenaltyMarkersForField(*score, round.currentHole, state.field, GolfPenaltyKind::Ob);
  view.hazardMinusEnabled = golfLatestPenaltyForFieldIs(*score, round.currentHole, state.field,
                                                       GolfPenaltyKind::Hazard);
  view.obMinusEnabled = golfLatestPenaltyForFieldIs(*score, round.currentHole, state.field,
                                                   GolfPenaltyKind::Ob);
  return view;
}

bool toggleGolfFairway(GolfRound& round) {
  GolfPlayerScore* score = nullptr;
  if (!currentScore(round, score)) return false;
  commitGolfPreview(round);
  golfSetFairwayHit(*score, round.currentHole, !golfFairwayHit(*score, round.currentHole));
  return true;
}

bool toggleMarkBunker(GolfRound& round) {
  GolfPlayerScore* score = nullptr;
  if (!currentScore(round, score)) return false;
  commitGolfPreview(round);
  golfSetGreensideBunker(*score, round.currentHole, !golfGreensideBunker(*score, round.currentHole));
  return true;
}

void selectMarkField(MarkSheetState& state, const GolfField field) {
  if (field != GolfField::Out100 && field != GolfField::In100) return;
  state.field = field;
  state.holeFull = false;
}

MarkMutationResult changeMarkPenalty(GolfRound& round, MarkSheetState& state,
                                     const GolfPenaltyKind kind, const bool increment) {
  GolfPlayerScore* score = nullptr;
  if (!currentScore(round, score)) return MarkMutationResult::NoChange;
  const bool seeded = commitGolfPreview(round);
  if (!increment && !golfLatestPenaltyForFieldIs(*score, round.currentHole, state.field, kind)) {
    return seeded ? MarkMutationResult::Changed : MarkMutationResult::NoChange;
  }
  const GolfPenaltyMutationStatus status =
      increment ? golfAppendPenalty(*score, round.currentHole, state.field, kind)
                : golfRemoveLatestPenalty(*score, round.currentHole, state.field);
  state.holeFull = status == GolfPenaltyMutationStatus::HoleFull;
  if (state.holeFull) return MarkMutationResult::HoleFull;
  return status == GolfPenaltyMutationStatus::Changed || seeded ? MarkMutationResult::Changed
                                                                 : MarkMutationResult::NoChange;
}
