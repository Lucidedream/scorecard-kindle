#include "ScoringScreen.h"

#include <stdio.h>

#include "PgmCanvas.h"
#include "MarkSheet.h"
#include "core/GolfPenalty.h"
#include "core/GolfStats.h"

namespace {

constexpr int HEADER_HEIGHT = 76;
constexpr int HOLE_STRIP_HEIGHT = 132;
constexpr int CONTEXT_HEIGHT = 56;
constexpr int TOTALS_HEIGHT = 150;
constexpr int FOOTER_HEIGHT = 160;
constexpr int NAV_WIDTH = 160;

uint8_t fieldIndex(const GolfField field) { return static_cast<uint8_t>(field); }

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

bool golfHoleIsLogged(const GolfPlayerScore& score, const uint8_t hole) {
  return hole < GolfRound::MAX_HOLES &&
         static_cast<uint16_t>(score.in100[hole]) + score.out100[hole] != 0;
}

ScoringLayout scoringLayout(const GolfField focused) {
  ScoringLayout layout{};
  layout.header = {0, 0, PgmCanvas::WIDTH, HEADER_HEIGHT};
  layout.holeStrip = {0, HEADER_HEIGHT, PgmCanvas::WIDTH, HOLE_STRIP_HEIGHT};
  layout.previous = {0, HEADER_HEIGHT, NAV_WIDTH, HOLE_STRIP_HEIGHT};
  layout.next = {PgmCanvas::WIDTH - NAV_WIDTH, HEADER_HEIGHT, NAV_WIDTH, HOLE_STRIP_HEIGHT};
  layout.context = {0, HEADER_HEIGHT + HOLE_STRIP_HEIGHT, PgmCanvas::WIDTH, CONTEXT_HEIGHT};
  layout.fairway = {PgmCanvas::WIDTH - 270, layout.context.y + 4, 232, layout.context.height - 8};
  layout.footer = {0, PgmCanvas::HEIGHT - FOOTER_HEIGHT, PgmCanvas::WIDTH, FOOTER_HEIGHT};
  layout.totals = {0, layout.footer.y - TOTALS_HEIGHT, PgmCanvas::WIDTH, TOTALS_HEIGHT};
  layout.thisHole = {0, layout.totals.y, PgmCanvas::WIDTH / 2, TOTALS_HEIGHT};
  layout.round = {PgmCanvas::WIDTH / 2, layout.totals.y, PgmCanvas::WIDTH / 2, TOTALS_HEIGHT};

  const int metricTop = layout.context.y + layout.context.height;
  const int metricHeight = layout.totals.y - metricTop;
  constexpr int NORMAL_HEIGHT = 230;
  const int focusedHeight = metricHeight - 2 * NORMAL_HEIGHT;
  int y = metricTop;
  for (uint8_t index = 0; index < 3; ++index) {
    const bool isFocused = index == fieldIndex(focused);
    const int height = isFocused ? focusedHeight : NORMAL_HEIGHT;
    layout.metrics[index] = {0, y, PgmCanvas::WIDTH, height};
    const int buttonWidth = isFocused ? 136 : 100;
    const int buttonHeight = isFocused ? 140 : 100;
    const int buttonY = y + (height - buttonHeight) / 2;
    const int plusX = PgmCanvas::WIDTH - 38 - buttonWidth;
    constexpr int BUTTON_GAP = 44;
    constexpr int VALUE_WIDTH = 120;
    const int valueRight = plusX - BUTTON_GAP;
    layout.plus[index] = {plusX, buttonY, buttonWidth, buttonHeight};
    layout.minus[index] = {valueRight - VALUE_WIDTH - BUTTON_GAP - buttonWidth, buttonY,
                           buttonWidth, buttonHeight};
    y += height;
  }

  constexpr int DIVIDER = 2;
  const int cellWidth = (PgmCanvas::WIDTH - 2 * DIVIDER) / 3;
  layout.menu = {0, layout.footer.y, cellWidth, FOOTER_HEIGHT};
  layout.mark = {cellWidth + DIVIDER, layout.footer.y, cellWidth, FOOTER_HEIGHT};
  layout.nextHole = {2 * (cellWidth + DIVIDER), layout.footer.y,
                     PgmCanvas::WIDTH - 2 * (cellWidth + DIVIDER), FOOTER_HEIGHT};
  return layout;
}

Rect scoringMinusRect(const ScoringLayout& layout, const uint8_t index, const int valueWidth) {
  if (index >= 3) return {};
  constexpr int BUTTON_GAP = 44;
  Rect minus = layout.minus[index];
  minus.x = layout.plus[index].x - BUTTON_GAP - valueWidth - BUTTON_GAP - minus.width;
  return minus;
}

ScoringView scoringView(const GolfRound& round, const GolfField focused) {
  ScoringView view{};
  view.layout = scoringLayout(focused);
  view.focused = focused;
  const GolfPlayerScore* score = nullptr;
  if (!currentScore(round, score)) return view;

  const uint8_t hole = round.currentHole;
  const GolfPlayer& player = round.players[round.currentPlayer];
  snprintf(view.header, sizeof(view.header), "%s · %s", player.name, round.courseName);
  if (round.par[hole] >= 3) {
    snprintf(view.hole, sizeof(view.hole), "HOLE %u · PAR %u", static_cast<unsigned>(hole + 1),
             round.par[hole]);
  } else {
    snprintf(view.hole, sizeof(view.hole), "HOLE %u", static_cast<unsigned>(hole + 1));
  }
  if (player.yards[hole] != 0 && round.hasSi) {
    snprintf(view.context, sizeof(view.context), "%u YD · SI %u", player.yards[hole], round.si[hole]);
  } else if (player.yards[hole] != 0) {
    snprintf(view.context, sizeof(view.context), "%u YD", player.yards[hole]);
  } else if (round.hasSi) {
    snprintf(view.context, sizeof(view.context), "SI %u", round.si[hole]);
  }

  GolfPlayerScore preview = *score;
  view.seeded = !golfHoleIsLogged(*score, hole) && round.par[hole] >= 3 &&
                seedGolfHoleAtPar(preview, hole, round.par[hole]);
  const GolfPlayerScore& display = view.seeded ? preview : *score;
  view.values[0] = display.putts[hole];
  view.values[1] = display.in100[hole];
  view.values[2] = display.out100[hole];
  view.thisHoleValue = golfHoleScore(round, display, hole);
  view.thru = golfThru(round, *score);
  view.hasPar = golfHasPar(round);
  view.fairwayVisible = !view.hasPar || round.par[hole] == 4 || round.par[hole] == 5;
  view.fairwayHit = golfFairwayHit(*score, hole);
  for (uint8_t index = 0; index < 3; ++index) {
    view.fieldMarked[index] = golfPenaltyMarkersForField(*score, hole, static_cast<GolfField>(index)) != 0;
  }
  view.bunkerMarked = golfGreensideBunker(*score, hole);
  const uint16_t penalties = golfPenaltyStrokesForHole(*score, hole);
  if (penalties == 0) snprintf(view.markLabel, sizeof(view.markLabel), "MARK");
  else snprintf(view.markLabel, sizeof(view.markLabel), "MARK +%u", penalties);
  snprintf(view.roundLabel, sizeof(view.roundLabel), "ROUND · THRU %u", view.thru);
  if (view.hasPar) {
    const int value = golfToPar(round, *score);
    if (value == 0) {
      snprintf(view.roundValue, sizeof(view.roundValue), "E");
    } else {
      snprintf(view.roundValue, sizeof(view.roundValue), "%+d", value);
    }
  } else {
    snprintf(view.roundValue, sizeof(view.roundValue), "%u", golfScore(round, *score));
  }
  return view;
}

bool commitGolfPreview(GolfRound& round) {
  GolfPlayerScore* score = nullptr;
  if (!currentScore(round, score) || golfHoleIsLogged(*score, round.currentHole)) return false;
  return seedGolfHoleAtPar(*score, round.currentHole, round.par[round.currentHole]);
}

GolfAdvanceResult commitAndAdvanceGolfTurn(GolfRound& round) {
  const bool committed = commitGolfPreview(round);
  return {committed, advanceGolfTurn(round)};
}

bool changeGolfField(GolfRound& round, const GolfField field, const bool decrement,
                     const uint16_t repeatCount) {
  GolfPlayerScore* score = nullptr;
  if (!currentScore(round, score) || repeatCount == 0) return false;
  bool changed = commitGolfPreview(round);
  for (uint16_t repeat = 0; repeat < repeatCount; ++repeat) {
    const GolfMutationResult result = decrement ? decrementGolfCounter(*score, round.currentHole, field)
                                                : incrementGolfCounter(*score, round.currentHole, field);
    changed = changed || result.changed;
    if (!result.changed) break;
  }
  return changed;
}
