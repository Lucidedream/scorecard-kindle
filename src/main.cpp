#include "HitTester.h"
#include "History.h"
#include "HoleReviewScreen.h"
#include "Keyboard.h"
#include "MarkSheet.h"
#include "PgmCanvas.h"
#include "ScorecardScreen.h"
#include "ScoringScreen.h"
#include "SetupScreens.h"
#include "SummaryScreen.h"
#include "TouchInput.h"
#include "UiStyle.h"
#include "core/Course.h"
#include "core/GolfStats.h"
#include "store/RoundArchive.h"
#include "store/GolfPaths.h"
#include "store/RoundStore.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

namespace {

constexpr char SCREEN_PATH[] = "/tmp/scorecard-screen.pgm";
constexpr char DIAGNOSTIC_SCREEN_PATH[] = "/mnt/us/scorecard/screen.pgm";
constexpr char RUNTIME_LOG_PATH[] = "/mnt/us/scorecard/runtime.log";
constexpr uint64_t IDLE_WRITE_MS = 5000;

enum Action {
  Previous = 1,
  Next,
  PuttsMinus,
  PuttsPlus,
  In100Minus,
  In100Plus,
  Out100Minus,
  Out100Plus,
  Menu,
  Mark,
  MenuScorecard,
  MenuHoleReview,
  MenuFinish,
  MenuAbandon,
  MenuBack,
  ConfirmNo,
  ConfirmYes,
  Fairway,
  MarkScrim,
  MarkOut100,
  MarkIn100,
  MarkBunker,
  MarkHazardMinus,
  MarkHazardPlus,
  MarkObMinus,
  MarkObPlus,
  MarkDone,
  ViewBack,
  PlayerChip,
  SummaryDone,
  HistoryPlayerFirst = 1000,
  HistoryRoundFirst = 1010,
  HistoryScorecard = 1100,
  HistoryReview,
  HistorySummary,
  HistoryDelete,
};

enum class Screen { Home, HistoryPlayers, HistoryRounds, HistoryRoundMenu, HistoryDetailUnavailable,
                    HistoryDeleteConfirm, Courses, PlayerCount, Roster, EditPlayer, TeeList, Keyboard, Scoring, Menu,
                    MarkSheet, Scorecard, Stats, HoleReview, ConfirmFinish, ConfirmAbandon, Summary,
                    ArchiveError };

struct HistoryContext {
  HistoryRow rows[GOLF_HISTORY_LIMIT];
  HistoryPlayer players[GolfRound::MAX_PLAYERS];
  const HistoryRow* playerRows[GOLF_HISTORY_LIMIT];
  HistoryReadResult read;
  size_t playerCount;
  size_t roundCount;
  size_t roundOffset;
  uint8_t selectedSlot;
  HistoryRow selectedRow;
  GolfRound loadedRound;
  bool loaded;
};

PgmCanvas canvas;
HitTester hitTester;
uint8_t viewedPlayer = 0;
uint8_t reviewedHole = 0;
char archivedFilename[GOLF_ARCHIVE_NAME_CAPACITY]{};
Screen viewReturnScreen = Screen::Menu;
HistoryContext history{};

void refreshHistory() {
  history.read = golfReadHistoryIndex(history.rows, GOLF_HISTORY_LIMIT);
  history.playerCount = golfHistoryPlayers(history.rows, history.read.count, history.players,
                                           GolfRound::MAX_PLAYERS);
  history.roundCount = golfHistoryRowsForPlayer(history.rows, history.read.count, history.selectedSlot,
                                                history.playerRows, GOLF_HISTORY_LIMIT);
}

uint64_t nowMs() {
  timespec now{};
  clock_gettime(CLOCK_MONOTONIC, &now);
  return static_cast<uint64_t>(now.tv_sec) * 1000U + static_cast<uint64_t>(now.tv_nsec) / 1000000U;
}

void makeDefaultRound(GolfRound& round) {
  round = {};
  applyCourse(round, GOLF_BUILT_IN_COURSES[0], GOLF_BUILT_IN_COURSES[0].tees[0].name);
  initializeGolfPlayerDefaults(round);
  golfSetTee(round.players[0], GOLF_BUILT_IN_COURSES[0].tees[0].name);
  for (uint8_t hole = 0; hole < round.holeCount; ++hole) {
    round.players[0].yards[hole] = GOLF_BUILT_IN_COURSES[0].tees[0].yards[hole];
  }
  round.currentHole = 0;
  round.currentPlayer = 0;
}

bool selfTest() {
  GolfRound source{};
  GolfRound loaded{};
  makeDefaultRound(source);
  source.dateYmd = static_cast<uint16_t>((26U << 9) | (9U << 5) | 10U);
  source.currentHole = 3;
  source.players[0].score.putts[0] = 2;
  source.players[0].score.in100[0] = 2;
  source.players[0].score.out100[0] = 2;
  if (!RoundStore::write(source)) return false;
  const GolfJsonResult result = RoundStore::read(loaded);
  const bool ok = result.status == GolfJsonStatus::Ok && memcmp(&source, &loaded, sizeof(source)) == 0;
  RoundStore::clear();
  if (ok) puts("OK");
  return ok;
}

const char* kindName(const TouchEvent::Kind kind) {
  switch (kind) {
    case TouchEvent::Kind::Tap: return "Tap";
    case TouchEvent::Kind::SwipeLeft: return "SwipeLeft";
    case TouchEvent::Kind::SwipeRight: return "SwipeRight";
    case TouchEvent::Kind::LongPress: return "LongPress";
  }
  return "Unknown";
}

void logEvent(const TouchEvent& event, const int action) {
  FILE* log = fopen(RUNTIME_LOG_PATH, "a");
  if (log == nullptr) return;
  fprintf(log, "TouchEvent kind=%s x=%d y=%d duration=%u action=%d\n", kindName(event.kind), event.x,
          event.y, event.durationMs, action);
  fclose(log);
}

void drawCentered(const Rect rect, const char* label, const TextSize size, const bool inverted = false,
                  const uint8_t shade = 0) {
  const int y = rect.y + (rect.height - canvas.lineHeight(size)) / 2;
  canvas.drawText(rect.x + rect.width / 2, y, label, size, TextAlign::Center, inverted, shade);
}

void drawPair(const Rect row, const char* label, const char* value, const TextSize valueSize,
              const uint8_t labelInk = INK_DIM, const uint8_t valueInk = 0) {
  const int labelY = row.y + (row.height - canvas.lineHeight(TextSize::Small)) / 2;
  const int valueY = row.y + (row.height - canvas.lineHeight(valueSize)) / 2;
  canvas.drawText(row.x + 48, labelY, label, TextSize::Small, TextAlign::Left, false, labelInk);
  canvas.drawText(row.x + row.width - 48, valueY, value, valueSize, TextAlign::Right, false, valueInk);
}

void drawScoring(const GolfRound& round, const GolfField focused) {
  const ScoringView view = scoringView(round, focused);
  const ScoringLayout& layout = view.layout;
  hitTester.clear();
  canvas.clear();

  canvas.drawText(38, layout.header.y + (layout.header.height - canvas.lineHeight(TextSize::Body)) / 2,
                  view.header, TextSize::Body);
  canvas.drawText(PgmCanvas::WIDTH - 38,
                  layout.header.y + (layout.header.height - canvas.lineHeight(TextSize::Small)) / 2,
                  "62%", TextSize::Small, TextAlign::Right);

  canvas.fillRect(0, layout.holeStrip.y, PgmCanvas::WIDTH, 2);
  drawCentered(layout.holeStrip, view.hole, TextSize::Display);
  hitTester.add(layout.previous, Action::Previous);
  hitTester.add(layout.next, Action::Next);
  hitTester.add({layout.previous.x + layout.previous.width, layout.holeStrip.y,
                 layout.holeStrip.width - layout.previous.width - layout.next.width,
                 layout.holeStrip.height}, Action::Menu);

  const int contextCenter = view.fairwayVisible ? (layout.fairway.x / 2) : (layout.context.width / 2);
  canvas.drawMonoText(contextCenter,
                      layout.context.y + (layout.context.height - canvas.lineHeight(TextSize::Small)) / 2,
                      view.context, TextSize::Small, TextAlign::Center, false, INK_DIM);
  if (view.fairwayVisible) {
    if (view.fairwayHit) canvas.fillRect(layout.fairway.x, layout.fairway.y, layout.fairway.width,
                                         layout.fairway.height);
    else canvas.drawRect(layout.fairway.x, layout.fairway.y, layout.fairway.width,
                         layout.fairway.height, 2);
    drawCentered(layout.fairway, "FAIRWAY", TextSize::Small, view.fairwayHit);
    hitTester.add(layout.fairway, Action::Fairway);
  }
  static constexpr char LABELS[3][16] = {"PUTTS", "INSIDE 100", "SCORE ZONE"};
  static constexpr Action MINUS_ACTIONS[3] = {Action::PuttsMinus, Action::In100Minus,
                                               Action::Out100Minus};
  static constexpr Action PLUS_ACTIONS[3] = {Action::PuttsPlus, Action::In100Plus,
                                              Action::Out100Plus};
  for (uint8_t index = 0; index < 3; ++index) {
    const Rect rect = layout.metrics[index];
    if (index != 0) canvas.fillRect(38, rect.y, PgmCanvas::WIDTH - 76, 2);
    const bool isFocused = index == static_cast<uint8_t>(focused);
    const int labelY = rect.y + (rect.height - canvas.lineHeight(TextSize::Small)) / 2;
    canvas.drawText(56, labelY, LABELS[index], TextSize::Small, TextAlign::Left, false,
                    isFocused ? 0 : INK_DIM);
    if (isFocused) canvas.fillRect(38, rect.y + rect.height / 2 - 50, 6, 100);
    if (view.fieldMarked[index] || (index == static_cast<uint8_t>(GolfField::In100) && view.bunkerMarked)) {
      const int flagX = 62 + canvas.measureText(LABELS[index], TextSize::Small);
      canvas.drawText(flagX, labelY, "!", TextSize::Small);
    }
    char value[8];
    snprintf(value, sizeof(value), "%u", view.values[index]);
    const TextSize size = isFocused ? TextSize::Display : TextSize::Body;
    const uint8_t shade = view.seeded ? INK_GHOST : 0;
    canvas.drawRect(layout.minus[index].x, layout.minus[index].y, layout.minus[index].width,
                    layout.minus[index].height, isFocused ? 5 : 3);
    canvas.drawRect(layout.plus[index].x, layout.plus[index].y, layout.plus[index].width,
                    layout.plus[index].height, isFocused ? 5 : 3);
    drawCentered(layout.minus[index], "-", TextSize::Body);
    drawCentered(layout.plus[index], "+", TextSize::Body);
    canvas.drawText(layout.plus[index].x - 44,
                    rect.y + (rect.height - canvas.lineHeight(size)) / 2,
                    value, size, TextAlign::Right, false, shade);
    hitTester.add(layout.minus[index], MINUS_ACTIONS[index]);
    hitTester.add(layout.plus[index], PLUS_ACTIONS[index]);
  }

  canvas.fillRect(0, layout.totals.y, PgmCanvas::WIDTH, 2);
  canvas.fillRect(PgmCanvas::WIDTH / 2 - 1, layout.totals.y + 22, 2, layout.totals.height - 44);
  char thisHole[8];
  snprintf(thisHole, sizeof(thisHole), "%u", view.thisHoleValue);
  drawPair({layout.thisHole.x, layout.thisHole.y, layout.thisHole.width, layout.thisHole.height},
           "THIS HOLE", thisHole, TextSize::Display, INK_DIM, view.seeded ? INK_GHOST : 0);
  drawPair({layout.round.x, layout.round.y, layout.round.width, layout.round.height},
           view.roundLabel, view.roundValue, TextSize::Display);

  canvas.fillRect(0, layout.footer.y, PgmCanvas::WIDTH, 2);
  canvas.fillRect(layout.menu.width, layout.footer.y, 2, layout.footer.height);
  canvas.fillRect(layout.mark.x + layout.mark.width, layout.footer.y, 2, layout.footer.height);
  canvas.fillRect(layout.nextHole.x, layout.nextHole.y, layout.nextHole.width, layout.nextHole.height);
  drawCentered(layout.menu, "MENU", TextSize::Body);
  drawCentered(layout.mark, view.markLabel, TextSize::Body);
  drawCentered(layout.nextHole, "NEXT HOLE >", TextSize::Body, true);
  hitTester.add(layout.menu, Action::Menu);
  hitTester.add(layout.mark, Action::Mark);
  hitTester.add(layout.nextHole, Action::Next);
}

void drawMarkSheet(const GolfRound& round, const MarkSheetState& state) {
  drawScoring(round, GolfField::Putts);
  const MarkSheetView view = markSheetView(round, state);
  const MarkSheetLayout& layout = view.layout;
  hitTester.clear();

  canvas.fillRect(layout.scrim.x, layout.scrim.y, layout.scrim.width, layout.scrim.height);
  canvas.fillRect(layout.sheet.x, layout.sheet.y, layout.sheet.width, layout.sheet.height, false);
  canvas.drawRect(layout.sheet.x, layout.sheet.y, layout.sheet.width, layout.sheet.height, 3);
  canvas.drawText(layout.title.x, layout.title.y + 18, view.title, TextSize::Display);
  canvas.drawText(layout.fieldLabel.x, layout.fieldLabel.y, "PENALTY SHOT FROM:", TextSize::Small,
                  TextAlign::Left, false, INK_DIM);

  const bool outSelected = view.field == GolfField::Out100;
  if (outSelected) canvas.fillRect(layout.out100Segment.x, layout.out100Segment.y,
                                   layout.out100Segment.width, layout.out100Segment.height);
  else canvas.drawRect(layout.out100Segment.x, layout.out100Segment.y,
                       layout.out100Segment.width, layout.out100Segment.height, 2);
  if (!outSelected) canvas.fillRect(layout.in100Segment.x, layout.in100Segment.y,
                                    layout.in100Segment.width, layout.in100Segment.height);
  else canvas.drawRect(layout.in100Segment.x, layout.in100Segment.y,
                       layout.in100Segment.width, layout.in100Segment.height, 2);
  drawCentered(layout.out100Segment, "SCORE ZONE", TextSize::Small, outSelected);
  drawCentered(layout.in100Segment, "INSIDE 100", TextSize::Small, !outSelected);

  canvas.drawRect(layout.bunkerRow.x, layout.bunkerRow.y, layout.bunkerRow.width,
                  layout.bunkerRow.height, 2);
  canvas.drawText(layout.bunkerRow.x + 24, layout.bunkerRow.y + 49, "Greenside bunker", TextSize::Body);
  const Rect checkbox{layout.bunkerRow.x + layout.bunkerRow.width - 104, layout.bunkerRow.y + 35, 72, 72};
  if (view.bunker) {
    canvas.fillRect(checkbox.x, checkbox.y, checkbox.width, checkbox.height);
    drawCentered(checkbox, "x", TextSize::Small, true);
  } else canvas.drawRect(checkbox.x, checkbox.y, checkbox.width, checkbox.height, 3);

  const auto drawPenaltyRow = [&](const Rect row, const Rect minus, const Rect plus, const char* label,
                                  const uint8_t count, const bool minusEnabled) {
    canvas.drawRect(row.x, row.y, row.width, row.height, 2);
    canvas.drawText(row.x + 24, row.y + 49, label, TextSize::Body);
    canvas.drawRect(minus.x, minus.y, minus.width, minus.height, 2);
    canvas.drawRect(plus.x, plus.y, plus.width, plus.height, 2);
    drawCentered(minus, "-", TextSize::Body, false, minusEnabled ? 0 : INK_GHOST);
    char value[8];
    snprintf(value, sizeof(value), "%u", count);
    const Rect countRect{minus.x + minus.width, row.y, plus.x - minus.x - minus.width, row.height};
    drawCentered(countRect, value, TextSize::Body);
    drawCentered(plus, "+", TextSize::Body);
  };
  drawPenaltyRow(layout.hazardRow, layout.hazardMinus, layout.hazardPlus, "Hazard  +1",
                 view.hazards, view.hazardMinusEnabled);
  drawPenaltyRow(layout.obRow, layout.obMinus, layout.obPlus, "Out of bounds  +2",
                 view.obs, view.obMinusEnabled);
  if (view.holeFull) drawCentered(layout.status, "HOLE IS FULL", TextSize::Small, false, INK_DIM);
  canvas.fillRect(layout.done.x, layout.done.y, layout.done.width, layout.done.height);
  drawCentered(layout.done, "DONE", TextSize::Body, true);

  hitTester.add(layout.scrim, Action::MarkScrim);
  hitTester.add(layout.out100Segment, Action::MarkOut100);
  hitTester.add(layout.in100Segment, Action::MarkIn100);
  hitTester.add(layout.bunkerRow, Action::MarkBunker);
  hitTester.add(layout.hazardMinus, Action::MarkHazardMinus);
  hitTester.add(layout.hazardPlus, Action::MarkHazardPlus);
  hitTester.add(layout.obMinus, Action::MarkObMinus);
  hitTester.add(layout.obPlus, Action::MarkObPlus);
  hitTester.add(layout.done, Action::MarkDone);
}

void drawMenu() {
  hitTester.clear();
  canvas.clear();
  canvas.drawText(42, 46, "ROUND MENU", TextSize::Display);
  const Rect scorecard{36, 176, 1000, 210};
  const Rect review{36, 386, 1000, 210};
  const Rect finish{36, 596, 1000, 210};
  const Rect abandon{36, 806, 1000, 210};
  const Rect back{0, 1258, PgmCanvas::WIDTH, 190};
  canvas.fillRect(scorecard.x, scorecard.y, scorecard.width, 2);
  canvas.fillRect(review.x, review.y, review.width, 2);
  canvas.fillRect(finish.x, finish.y, finish.width, 2);
  canvas.fillRect(abandon.x, abandon.y, abandon.width, 2);
  canvas.fillRect(abandon.x, abandon.y + abandon.height - 2, abandon.width, 2);
  canvas.fillRect(back.x, back.y, back.width, 2);
  const auto menuLabel = [&](const Rect row, const char* label) {
    canvas.drawText(row.x + 20, row.y + (row.height - canvas.lineHeight(TextSize::Body)) / 2,
                    label, TextSize::Body);
  };
  menuLabel(scorecard, "View scorecard");
  menuLabel(review, "Hole review");
  menuLabel(finish, "Finish round");
  menuLabel(abandon, "Abandon round");
  drawCentered(back, "<  BACK TO SCORING", TextSize::Body);
  hitTester.add(scorecard, Action::MenuScorecard);
  hitTester.add(review, Action::MenuHoleReview);
  hitTester.add(finish, Action::MenuFinish);
  hitTester.add(abandon, Action::MenuAbandon);
  hitTester.add(back, Action::MenuBack);
}

void drawViewHeader(const char* title, const char* value, const char* player, const bool showPlayer) {
  constexpr int HEADER_Y = 42;
  canvas.drawText(36, HEADER_Y, title, TextSize::Body);
  canvas.drawText(PgmCanvas::WIDTH - 36, HEADER_Y, value, TextSize::Body, TextAlign::Right);
  if (showPlayer) {
    const Rect chip{PgmCanvas::WIDTH - 350, 100, 314, 104};
    canvas.drawRect(chip.x, chip.y, chip.width, chip.height, 2);
    drawCentered(chip, player, TextSize::Small);
    hitTester.add(chip, Action::PlayerChip);
  }
}

void drawBottomBack(const char* hint = "<  BACK") {
  const Rect back{0, 1300, PgmCanvas::WIDTH, 148};
  canvas.fillRect(0, back.y, PgmCanvas::WIDTH, 2);
  drawCentered(back, hint, TextSize::Body);
  hitTester.add(back, Action::ViewBack);
}

void drawScorecard(const GolfRound& round) {
  const ScorecardView view = scorecardView(round, viewedPlayer);
  hitTester.clear();
  canvas.clear();
  drawViewHeader(view.header, view.roundValue, view.player,
                 view.playerCount > 1 && viewReturnScreen != Screen::HistoryRoundMenu);
  constexpr int MARGIN = 28;
  constexpr int LABEL_W = 116;
  constexpr int DATA_W = (PgmCanvas::WIDTH - 2 * MARGIN - LABEL_W) / 10;
  constexpr int TOP = 218;
  constexpr int HEADER_H = 52;
  constexpr int ROW_H = 60;
  constexpr int BLOCK_GAP = 28;
  static constexpr char ROWS[6][12] = {"PAR", "SCORE", "PUTTS", "IN 100", "ZONE", "PEN"};
  const uint8_t firstRow = view.hasPar ? 0 : 1;
  const int shownRows = 6 - firstRow;
  const int blockHeight = HEADER_H + shownRows * ROW_H;
  const auto drawNine = [&](const int top, const uint8_t firstHole, const char* blockLabel,
                            const char* subtotalLabel, const char totals[][8]) {
    canvas.drawText(MARGIN, top + (HEADER_H - canvas.lineHeight(TextSize::Small)) / 2,
                    blockLabel, TextSize::Small);
    for (uint8_t column = 0; column < 9; ++column) {
      char holeLabel[4];
      snprintf(holeLabel, sizeof(holeLabel), "%u", firstHole + column + 1);
      const int center = MARGIN + LABEL_W + column * DATA_W + DATA_W / 2;
      canvas.drawMonoText(center, top + (HEADER_H - canvas.lineHeight(TextSize::Small)) / 2,
                          holeLabel, TextSize::Small, TextAlign::Center);
    }
    const int totalCenter = MARGIN + LABEL_W + 9 * DATA_W + DATA_W / 2;
    canvas.drawMonoText(totalCenter, top + (HEADER_H - canvas.lineHeight(TextSize::Small)) / 2,
                        subtotalLabel, TextSize::Small, TextAlign::Center);
    canvas.fillRect(MARGIN, top + HEADER_H - 2, PgmCanvas::WIDTH - 2 * MARGIN, 2);
    uint8_t displayRow = 0;
    for (uint8_t row = firstRow; row < 6; ++row, ++displayRow) {
      const int y = top + HEADER_H + displayRow * ROW_H;
      const bool scoreRow = row == static_cast<uint8_t>(ScorecardMetric::Score);
      if (displayRow != 0) canvas.fillRect(MARGIN, y, PgmCanvas::WIDTH - 2 * MARGIN, scoreRow ? 4 : 1);
      const TextSize numberSize = scoreRow ? TextSize::Body : TextSize::Small;
      canvas.drawText(MARGIN, y + (ROW_H - canvas.lineHeight(TextSize::Small)) / 2,
                      ROWS[row], TextSize::Small, TextAlign::Left, false,
                      scoreRow ? 0 : INK_DIM);
      for (uint8_t column = 0; column < 9; ++column) {
        const int center = MARGIN + LABEL_W + column * DATA_W + DATA_W / 2;
        canvas.drawMonoText(center, y + (ROW_H - canvas.lineHeight(numberSize)) / 2,
                            view.cells[row][firstHole + column], numberSize, TextAlign::Center);
      }
      canvas.drawMonoText(totalCenter, y + (ROW_H - canvas.lineHeight(numberSize)) / 2,
                          totals[row], numberSize, TextAlign::Center);
    }
    canvas.fillRect(MARGIN + LABEL_W - 10, top, 2, blockHeight);
    canvas.fillRect(MARGIN + LABEL_W + 9 * DATA_W, top, 2, blockHeight);
    canvas.fillRect(MARGIN, top + blockHeight - 2, PgmCanvas::WIDTH - 2 * MARGIN, 2);
  };
  drawNine(TOP, 0, "FRONT", "OUT", view.out);
  const int backTop = TOP + blockHeight + BLOCK_GAP;
  drawNine(backTop, 9, "BACK", "IN", view.in);
  const Rect totalLine{MARGIN, backTop + blockHeight + 18, PgmCanvas::WIDTH - 2 * MARGIN, 104};
  canvas.fillRect(totalLine.x, totalLine.y, totalLine.width, 4);
  drawPair(totalLine, "TOTAL", view.roundValue, TextSize::Display, 0);
  canvas.drawText(PgmCanvas::WIDTH / 2, 1218, "SWIPE FOR STATS", TextSize::Small, TextAlign::Center,
                  false, INK_DIM);
  drawBottomBack();
}

void drawStats(const GolfRound& round) {
  const StatsView view = statsView(round, viewedPlayer);
  hitTester.clear();
  canvas.clear();
  drawViewHeader(view.header, view.score, view.player,
                 view.playerCount > 1 && viewReturnScreen != Screen::HistoryRoundMenu);
  static constexpr char LABELS[10][20] = {"Putts", "1-putts", "3-putts", "Long game", "Short game",
                                          "Putting", "Penalties", "Fairways", "Greens", "Worst holes"};
  const char* values[10] = {view.putts, view.onePutts, view.threePutts, view.longGame, view.shortGame,
                            view.putting, view.penalties, view.fairways, view.greens, view.worst};
  const uint8_t rowCount = view.hasPar ? 10 : 9;
  int y = 220;
  constexpr int ROW_HEIGHT = 99;
  for (uint8_t row = 0; row < rowCount; ++row, y += ROW_HEIGHT) {
    canvas.fillRect(36, y, PgmCanvas::WIDTH - 72, 2);
    drawPair({4, y, PgmCanvas::WIDTH - 8, ROW_HEIGHT}, LABELS[row], values[row], TextSize::Small);
  }
  canvas.drawText(PgmCanvas::WIDTH / 2, 1215, "SWIPE FOR SCORECARD", TextSize::Small,
                  TextAlign::Center, false, INK_DIM);
  drawBottomBack();
}

void drawHoleReview(const GolfRound& round) {
  const HoleReviewView view = holeReviewView(round, viewedPlayer, reviewedHole);
  hitTester.clear();
  canvas.clear();
  const Rect previous{0, 0, 180, 190};
  const Rect next{PgmCanvas::WIDTH - 180, 0, 180, 190};
  drawCentered({0, 0, PgmCanvas::WIDTH, 190}, view.strip, TextSize::Display);
  hitTester.add(previous, Action::Previous);
  hitTester.add(next, Action::Next);
  canvas.fillRect(0, 188, PgmCanvas::WIDTH, 2);
  const Rect hero{0, 190, PgmCanvas::WIDTH, 390};
  canvas.drawMonoText(PgmCanvas::WIDTH / 2, hero.y + 66, view.context, TextSize::Small,
                      TextAlign::Center, false, INK_DIM);
  canvas.drawText(PgmCanvas::WIDTH / 2,
                  hero.y + (hero.height - canvas.lineHeight(TextSize::Display)) / 2 + 34,
                  view.hero, TextSize::Display, TextAlign::Center);
  static constexpr char LABELS[3][20] = {"PUTTS", "INSIDE 100", "TO SCORE ZONE"};
  const char* values[3] = {view.putts, view.in100, view.zone};
  for (uint8_t row = 0; row < 3; ++row) {
    const int y = 600 + row * 155;
    canvas.fillRect(80, y, PgmCanvas::WIDTH - 160, 2);
    drawPair({52, y, PgmCanvas::WIDTH - 104, 155}, LABELS[row], values[row], TextSize::Body);
  }
  if (view.hasPenalty) canvas.drawText(PgmCanvas::WIDTH / 2, 1080, view.penalty, TextSize::Body, TextAlign::Center);
  if (view.marks[0] != '\0') canvas.drawText(PgmCanvas::WIDTH / 2, 1165, view.marks, TextSize::Small,
                                             TextAlign::Center, false, INK_DIM);
  drawBottomBack();
}

void drawFinishConfirmation(const GolfRound& round) {
  hitTester.clear();
  canvas.clear();
  const GolfPlayerScore& score = round.players[round.currentPlayer].score;
  char detail[80];
  if (golfHasPar(round)) {
    const int toPar = golfToPar(round, score);
    if (toPar == 0) snprintf(detail, sizeof(detail), "Thru %u, E.", golfThru(round, score));
    else snprintf(detail, sizeof(detail), "Thru %u, %+d.", golfThru(round, score), toPar);
  } else snprintf(detail, sizeof(detail), "Thru %u, score %u.", golfThru(round, score), golfScore(round, score));
  canvas.drawText(PgmCanvas::WIDTH / 2, 246, "FINISH THIS ROUND?", TextSize::Display, TextAlign::Center);
  canvas.drawText(PgmCanvas::WIDTH / 2, 396, detail, TextSize::Body, TextAlign::Center);
  const Rect no{36, 650, 490, 220};
  const Rect yes{546, 650, 490, 220};
  canvas.drawRect(no.x, no.y, no.width, no.height, 2);
  canvas.fillRect(yes.x, yes.y, yes.width, yes.height);
  drawCentered(no, "NO", TextSize::Body);
  drawCentered(yes, "YES", TextSize::Body, true);
  hitTester.add(no, Action::ConfirmNo);
  hitTester.add(yes, Action::ConfirmYes);
}

void drawSummary(const GolfRound& round) {
  const SummaryView view = summaryView(round, viewedPlayer);
  hitTester.clear();
  canvas.clear();
  drawViewHeader(view.header, "", view.player,
                 view.playerCount > 1 && viewReturnScreen != Screen::HistoryRoundMenu);
  canvas.drawText(PgmCanvas::WIDTH / 2, 206, view.score, TextSize::Display, TextAlign::Center);
  canvas.fillRect(420, 328, 232, 5);
  static constexpr char LABELS[7][18] = {"TO PAR", "PUTTS", "INSIDE 100", "LONG GAME", "PENALTIES",
                                         "FAIRWAYS", "GREENS"};
  const char* values[7] = {view.toPar, view.putts, view.in100, view.longGame, view.penalties,
                           view.fairways, view.greens};
  const uint8_t first = view.hasPar ? 0 : 1;
  uint8_t shown = 0;
  for (uint8_t index = first; index < 7; ++index, ++shown) {
    const uint8_t col = shown % 2;
    const uint8_t row = shown / 2;
    const Rect cell{36 + col * 510, 400 + row * 190, 490, 170};
    if (row != 0) canvas.fillRect(cell.x, cell.y, cell.width, 2);
    if (col != 0) canvas.fillRect(cell.x, cell.y + 22, 2, cell.height - 44);
    drawPair({cell.x - 24, cell.y, cell.width + 48, cell.height}, LABELS[index], values[index],
             TextSize::Body);
  }
  if (viewReturnScreen == Screen::HistoryRoundMenu) {
    drawBottomBack();
  } else {
    char saved[160];
    snprintf(saved, sizeof(saved), "Saved to rounds/%s.", archivedFilename);
    canvas.drawText(PgmCanvas::WIDTH / 2, 1190, saved, TextSize::Small, TextAlign::Center, false, INK_DIM);
    const Rect done{0, 1300, PgmCanvas::WIDTH, 148};
    canvas.fillRect(done.x, done.y, done.width, done.height);
    drawCentered(done, "DONE", TextSize::Body, true);
    hitTester.add(done, Action::SummaryDone);
  }
}

void drawArchiveError() {
  hitTester.clear();
  canvas.clear();
  canvas.drawText(PgmCanvas::WIDTH / 2, 270, "COULDN'T SAVE THE ROUND", TextSize::Display, TextAlign::Center);
  canvas.drawText(PgmCanvas::WIDTH / 2, 430, "It's still open.", TextSize::Body, TextAlign::Center);
  drawBottomBack("<  BACK TO SCORING");
}

void drawAbandonConfirmation() {
  hitTester.clear();
  canvas.clear();
  canvas.drawText(PgmCanvas::WIDTH / 2, 246, "ABANDON ROUND?", TextSize::Display, TextAlign::Center);
  canvas.drawText(PgmCanvas::WIDTH / 2, 396, "This clears the current scorecard.", TextSize::Body,
                  TextAlign::Center);
  const Rect no{36, 650, 490, 220};
  const Rect yes{546, 650, 490, 220};
  canvas.drawRect(no.x, no.y, no.width, no.height, 2);
  canvas.fillRect(yes.x, yes.y, yes.width, yes.height);
  drawCentered(no, "NO", TextSize::Body);
  drawCentered(yes, "YES", TextSize::Body, true);
  hitTester.add(no, Action::ConfirmNo);
  hitTester.add(yes, Action::ConfirmYes);
}

void drawHistoryBack() { drawBottomBack(); }

void drawHistoryPlayers() {
  hitTester.clear();
  canvas.clear();
  canvas.drawText(38, 48, "HISTORY", TextSize::Display);
  if (!history.read.readable) {
    canvas.drawText(PgmCanvas::WIDTH / 2, 410, "Couldn't read your rounds.", TextSize::Body, TextAlign::Center);
  } else if (history.playerCount == 0) {
    canvas.drawText(PgmCanvas::WIDTH / 2, 410, "No rounds yet", TextSize::Body, TextAlign::Center);
  } else {
    for (size_t i = 0; i < history.playerCount; ++i) {
      const Rect row{36, 170 + static_cast<int>(i) * 210, 1000, 210};
      canvas.fillRect(row.x, row.y, row.width, 2);
      canvas.drawText(row.x + 24, row.y + 45, history.players[i].name, TextSize::Body);
      char rounds[32];
      snprintf(rounds, sizeof(rounds), "%zu round%s", history.players[i].roundCount,
               history.players[i].roundCount == 1 ? "" : "s");
      canvas.drawText(row.x + 24, row.y + 119, rounds, TextSize::Small, TextAlign::Left, false, INK_DIM);
      canvas.drawText(row.x + row.width - 30, row.y + 73, ">", TextSize::Body, TextAlign::Right);
      hitTester.add(row, Action::HistoryPlayerFirst + static_cast<int>(i));
    }
  }
  drawHistoryBack();
}

void drawHistoryRounds() {
  hitTester.clear();
  canvas.clear();
  const HistoryPlayer* selected = nullptr;
  for (size_t i = 0; i < history.playerCount; ++i)
    if (history.players[i].slot == history.selectedSlot) selected = &history.players[i];
  char header[80];
  snprintf(header, sizeof(header), "%s · %zu rounds", selected == nullptr ? "PLAYER" : selected->name,
           history.roundCount);
  canvas.drawText(38, 48, header, TextSize::Display);
  const size_t remaining = history.roundCount > history.roundOffset ? history.roundCount - history.roundOffset : 0;
  const size_t shown = remaining < 8 ? remaining : 8;
  for (size_t i = 0; i < shown; ++i) {
    const HistoryRow& item = *history.playerRows[history.roundOffset + i];
    const Rect row{36, 150 + static_cast<int>(i) * 138, 1000, 138};
    canvas.fillRect(row.x, row.y, row.width, 2);
    canvas.drawText(row.x + 20, row.y + 24, item.course, TextSize::Body);
    char detail[48];
    snprintf(detail, sizeof(detail), "%s%s%u holes", item.date,
             item.date[0] == '\0' ? "" : " · ", item.holes);
    canvas.drawText(row.x + 20, row.y + 83, detail, TextSize::Small, TextAlign::Left, false, INK_DIM);
    char score[12], toPar[12];
    snprintf(score, sizeof(score), "%u", item.strokes);
    if (item.par == 0) snprintf(toPar, sizeof(toPar), "—");
    else if (item.strokes == item.par) snprintf(toPar, sizeof(toPar), "E");
    else snprintf(toPar, sizeof(toPar), "%+d", static_cast<int>(item.strokes) - item.par);
    canvas.drawText(row.x + row.width - 24, row.y + 18, score, TextSize::Body, TextAlign::Right);
    canvas.drawText(row.x + row.width - 24, row.y + 80, toPar, TextSize::Small, TextAlign::Right, false, INK_DIM);
    hitTester.add(row, Action::HistoryRoundFirst + static_cast<int>(i));
  }
  if (history.roundCount > 8)
    canvas.drawText(PgmCanvas::WIDTH / 2, 1250, "SWIPE FOR MORE", TextSize::Small,
                    TextAlign::Center, false, INK_DIM);
  else if (history.read.olderRoundsExist)
    canvas.drawText(PgmCanvas::WIDTH / 2, 1250, "Older rounds not shown", TextSize::Small,
                    TextAlign::Center, false, INK_DIM);
  drawHistoryBack();
}

void drawHistoryRoundMenu() {
  hitTester.clear();
  canvas.clear();
  canvas.drawText(38, 48, "ROUND", TextSize::Display);
  char subtitle[100];
  snprintf(subtitle, sizeof(subtitle), "%s%s%s", history.selectedRow.course,
           history.selectedRow.date[0] == '\0' ? "" : " · ", history.selectedRow.date);
  canvas.drawText(38, 120, subtitle, TextSize::Small, TextAlign::Left, false, INK_DIM);
  static constexpr const char* labels[4] = {"View scorecard", "Hole review", "Round summary", "Delete round"};
  static constexpr int actions[4] = {Action::HistoryScorecard, Action::HistoryReview,
                                     Action::HistorySummary, Action::HistoryDelete};
  for (uint8_t i = 0; i < 4; ++i) {
    const Rect row{36, 220 + i * 190, 1000, 190};
    canvas.fillRect(row.x, row.y, row.width, 2);
    canvas.drawText(row.x + 24, row.y + 58, labels[i], TextSize::Body);
    hitTester.add(row, actions[i]);
  }
  drawHistoryBack();
}

void drawHistoryUnavailable() {
  hitTester.clear();
  canvas.clear();
  canvas.drawText(PgmCanvas::WIDTH / 2, 330, "Hole-by-hole detail unavailable.", TextSize::Body,
                  TextAlign::Center);
  drawHistoryBack();
}

void drawHistoryDeleteConfirm() {
  hitTester.clear();
  canvas.clear();
  const size_t shared = golfHistoryFileRowCount(history.rows, history.read.count, history.selectedRow.file);
  canvas.drawText(PgmCanvas::WIDTH / 2, 246,
                  shared > 1 ? "REMOVE THIS PLAYER?" : "DELETE THIS ROUND?",
                  TextSize::Display, TextAlign::Center);
  char detail[160];
  if (shared > 1) snprintf(detail, sizeof(detail), "Remove %s from this round? The other players keep their scores.",
                           history.selectedRow.playerName);
  else snprintf(detail, sizeof(detail), "Delete this round?");
  canvas.drawText(PgmCanvas::WIDTH / 2, 410, detail, TextSize::Body, TextAlign::Center);
  const Rect no{36, 650, 490, 220}, yes{546, 650, 490, 220};
  canvas.drawRect(no.x, no.y, no.width, no.height, 2);
  canvas.fillRect(yes.x, yes.y, yes.width, yes.height);
  drawCentered(no, "NO", TextSize::Body);
  drawCentered(yes, "YES", TextSize::Body, true);
  hitTester.add(no, Action::ConfirmNo);
  hitTester.add(yes, Action::ConfirmYes);
}

void drawHistoryFallbackSummary() {
  const SummaryView view = golfHistorySummaryView(history.selectedRow);
  hitTester.clear();
  canvas.clear();
  drawViewHeader(view.header, "", view.player, false);
  canvas.drawText(PgmCanvas::WIDTH / 2, 206, view.score, TextSize::Display, TextAlign::Center);
  static constexpr char LABELS[5][18] = {"TO PAR", "PUTTS", "INSIDE 100", "LONG GAME", "PENALTIES"};
  const char* values[5] = {view.toPar, view.putts, view.in100, view.longGame, view.penalties};
  const uint8_t first = view.hasPar ? 0 : 1;
  for (uint8_t i = first, shown = 0; i < 5; ++i, ++shown) {
    const Rect cell{36, 390 + shown * 145, 1000, 145};
    canvas.fillRect(cell.x, cell.y, cell.width, 2);
    drawPair(cell, LABELS[i], values[i], TextSize::Body);
  }
  canvas.drawText(PgmCanvas::WIDTH / 2, 1190, "Hole-by-hole detail unavailable.", TextSize::Small,
                  TextAlign::Center, false, INK_DIM);
  drawHistoryBack();
}

SetupScreen setupScreen(const Screen screen) {
  switch (screen) {
    case Screen::Home: return SetupScreen::Home;
    case Screen::Courses: return SetupScreen::Courses;
    case Screen::PlayerCount: return SetupScreen::PlayerCount;
    case Screen::Roster: return SetupScreen::Roster;
    case Screen::EditPlayer: return SetupScreen::EditPlayer;
    case Screen::TeeList: return SetupScreen::TeeList;
    default: return SetupScreen::Home;
  }
}

bool paint(const char* path, const Screen screen, const GolfRound& round, const GolfField focused,
           const SetupState& setup, const HomeSummary& home, const KeyboardState& keyboard,
           const MarkSheetState& markState, const bool showOnDevice, const bool fullRefresh) {
  switch (screen) {
    case Screen::Home:
    case Screen::Courses:
    case Screen::PlayerCount:
    case Screen::Roster:
    case Screen::EditPlayer:
    case Screen::TeeList: drawSetupScreen(canvas, hitTester, setupScreen(screen), setup, home); break;
    case Screen::HistoryPlayers: drawHistoryPlayers(); break;
    case Screen::HistoryRounds: drawHistoryRounds(); break;
    case Screen::HistoryRoundMenu: drawHistoryRoundMenu(); break;
    case Screen::HistoryDetailUnavailable:
      if (viewReturnScreen == Screen::Summary) drawHistoryFallbackSummary();
      else drawHistoryUnavailable();
      break;
    case Screen::HistoryDeleteConfirm: drawHistoryDeleteConfirm(); break;
    case Screen::Keyboard: drawKeyboard(canvas, hitTester, keyboard); break;
    case Screen::Scoring: drawScoring(round, focused); break;
    case Screen::MarkSheet: drawMarkSheet(round, markState); break;
    case Screen::Menu: drawMenu(); break;
    case Screen::Scorecard: drawScorecard(round); break;
    case Screen::Stats: drawStats(round); break;
    case Screen::HoleReview: drawHoleReview(round); break;
    case Screen::ConfirmFinish: drawFinishConfirmation(round); break;
    case Screen::ConfirmAbandon: drawAbandonConfirmation(); break;
    case Screen::Summary: drawSummary(round); break;
    case Screen::ArchiveError: drawArchiveError(); break;
  }
  if (!canvas.write(path)) return false;
  if (!showOnDevice) return true;
  canvas.write(DIAGNOSTIC_SCREEN_PATH);
  const char* command = fullRefresh
                            ? "/mnt/us/libkh/bin/fbink -c -f -W GC16 -i /tmp/scorecard-screen.pgm "
                              ">/mnt/us/scorecard/fbink.log 2>&1"
                            : "/mnt/us/libkh/bin/fbink -c -W DU -i /tmp/scorecard-screen.pgm "
                              ">/mnt/us/scorecard/fbink.log 2>&1";
  return system(command) == 0;
}

bool paintDevice(const Screen screen, const GolfRound& round, const GolfField focused, const SetupState& setup,
                 const HomeSummary& home, const KeyboardState& keyboard, const bool fullRefresh,
                 const MarkSheetState& markState, unsigned int& paints) {
  if (!paint(SCREEN_PATH, screen, round, focused, setup, home, keyboard, markState, true, fullRefresh)) return false;
  ++paints;
  return true;
}

void makeGoldenRound(GolfRound& round) {
  makeDefaultRound(round);
  for (uint8_t hole = 0; hole < 6; ++hole) {
    round.players[0].score.putts[hole] = static_cast<uint8_t>(hole % 3 + 1);
    round.players[0].score.in100[hole] = static_cast<uint8_t>(hole % 3 + 2);
    round.players[0].score.out100[hole] = static_cast<uint8_t>(round.par[hole] - 2);
  }
  round.currentHole = 6;
  snprintf(archivedFilename, sizeof(archivedFilename), "round-0001-municipal-links.json");
}

bool startSetupRound(const SetupState& setup, GolfRound& round) {
  if (!buildRoundFromSetup(setup, round)) return false;
  const time_t raw = time(nullptr);
  const tm* date = localtime(&raw);
  if (date != nullptr) {
    round.dateYmd = static_cast<uint16_t>(((date->tm_year - 100) << 9) |
                                          ((date->tm_mon + 1) << 5) | date->tm_mday);
  }
  return RoundStore::write(round);
}

}  // namespace

int main(const int argc, char** argv) {
  if (argc == 2 && strcmp(argv[1], "--selftest") == 0) return selfTest() ? 0 : 1;
  if (argc == 3 && strcmp(argv[1], "--render") == 0) {
    GolfRound round{};
    SetupState setup{};
    HomeSummary home{};
    KeyboardState keyboard{};
    initializeSetup(setup);
    initializeKeyboard(keyboard, "Noah");
    makeGoldenRound(round);
    const MarkSheetState markState = initialMarkSheetState();
    return paint(argv[2], Screen::Scoring, round, GolfField::Putts, setup, home, keyboard, markState,
                 false, true) ? 0 : 1;
  }
  if (argc == 4 && strcmp(argv[1], "--render") == 0) {
    GolfRound round{};
    SetupState setup{};
    HomeSummary home{};
    KeyboardState keyboard{};
    initializeSetup(setup);
    initializeKeyboard(keyboard, "Noah");
    makeGoldenRound(round);
    MarkSheetState markState = initialMarkSheetState();
    Screen screen = Screen::Home;
    history = {};
    history.read.readable = true;
    history.read.count = 3;
    history.playerCount = 2;
    history.players[0] = {0, "Noah", 2};
    history.players[1] = {1, "Maya", 1};
    history.selectedSlot = 0;
    for (size_t i = 0; i < 3; ++i) {
      HistoryRow& item = history.rows[i];
      snprintf(item.date, sizeof(item.date), "2026-09-%02zu", 8 + i);
      snprintf(item.course, sizeof(item.course), "%s", i == 2 ? "Harbour Golf Club" : "Municipal Links");
      item.holes = 18;
      item.playerSlot = static_cast<uint8_t>(i == 0 ? 1 : 0);
      snprintf(item.playerName, sizeof(item.playerName), "%s", item.playerSlot == 0 ? "Noah" : "Maya");
      item.strokes = static_cast<uint16_t>(86 + i);
      item.par = 72;
      snprintf(item.file, sizeof(item.file), "round-%04zu-course.json", i + 1);
    }
    history.playerRows[0] = &history.rows[2];
    history.playerRows[1] = &history.rows[1];
    history.roundCount = 2;
    history.selectedRow = history.rows[2];
    if (strcmp(argv[2], "keyboard") == 0) screen = Screen::Keyboard;
    else if (strcmp(argv[2], "scoring") == 0) screen = Screen::Scoring;
    else if (strcmp(argv[2], "mark-sheet") == 0) screen = Screen::MarkSheet;
    else if (strcmp(argv[2], "scorecard") == 0) screen = Screen::Scorecard;
    else if (strcmp(argv[2], "stats") == 0) screen = Screen::Stats;
    else if (strcmp(argv[2], "hole-review") == 0) {
      screen = Screen::HoleReview;
      reviewedHole = round.currentHole == 0 ? 0 : static_cast<uint8_t>(round.currentHole - 1);
    }
    else if (strcmp(argv[2], "summary") == 0) screen = Screen::Summary;
    else if (strcmp(argv[2], "history-players") == 0) screen = Screen::HistoryPlayers;
    else if (strcmp(argv[2], "history-rounds") == 0) screen = Screen::HistoryRounds;
    else if (strcmp(argv[2], "history-round-menu") == 0) screen = Screen::HistoryRoundMenu;
    else if (strcmp(argv[2], "marked") == 0) {
      screen = Screen::Scoring;
      commitGolfPreview(round);
      GolfPlayerScore& score = round.players[round.currentPlayer].score;
      golfSetFairwayHit(score, round.currentHole, true);
      golfSetGreensideBunker(score, round.currentHole, true);
      golfAppendPenalty(score, round.currentHole, GolfField::Out100, GolfPenaltyKind::Hazard);
      golfAppendPenalty(score, round.currentHole, GolfField::In100, GolfPenaltyKind::Ob);
    }
    else if (strcmp(argv[2], "home") != 0) return 2;
    return paint(argv[3], screen, round, GolfField::Putts, setup, home, keyboard, markState, false,
                 true) ? 0 : 1;
  }

  GolfRound round{};
  const bool hasRound = RoundStore::read(round).status == GolfJsonStatus::Ok;
  SetupState setup{};
  HomeSummary home{};
  KeyboardState keyboard{};
  initializeSetup(setup);
  readHomeSummary(home);
  GolfField focused = GolfField::Putts;
  MarkSheetState markState = initialMarkSheetState();
  Screen screen = hasRound ? Screen::Scoring : Screen::Home;
  viewedPlayer = round.currentPlayer;
  TouchInput input;
  if (!input.openDevice()) {
    fprintf(stderr, "Could not find the Kindle touchscreen\n");
    return 2;
  }

  FILE* log = fopen(RUNTIME_LOG_PATH, "a");
  if (log != nullptr) {
    fprintf(log, "Started with touchscreen: %s\n", input.deviceName());
    fclose(log);
  }

  unsigned int paints = 0;
  bool counterDirty = false;
  uint64_t counterChangedAt = 0;
  if (!paintDevice(screen, round, focused, setup, home, keyboard, true, markState, paints)) return 3;
  while (true) {
    int timeout = -1;
    if (counterDirty) {
      const uint64_t elapsed = nowMs() - counterChangedAt;
      timeout = elapsed >= IDLE_WRITE_MS ? 0 : static_cast<int>(IDLE_WRITE_MS - elapsed);
    }
    TouchEvent event{};
    const TouchWaitResult wait = input.waitForEvent(event, timeout);
    if (wait == TouchWaitResult::Timeout) {
      RoundStore::write(round);
      counterDirty = false;
      continue;
    }
    if (wait == TouchWaitResult::Error) return 4;
    const int action = hitTester.hitTest(event.x, event.y);
    logEvent(event, action);

    bool repaint = false;
    bool forceGc = false;
    if (screen == Screen::Home && event.kind == TouchEvent::Kind::Tap) {
      if (action == SetupNewRound) {
        initializeSetup(setup);
        screen = Screen::Courses;
        repaint = true;
        forceGc = true;
      } else if (action == SetupHistory) {
        refreshHistory();
        screen = Screen::HistoryPlayers;
        repaint = true;
        forceGc = true;
      }
    } else if (screen == Screen::HistoryPlayers && event.kind == TouchEvent::Kind::Tap) {
      if (action == Action::ViewBack) screen = Screen::Home;
      else if (action >= Action::HistoryPlayerFirst &&
               action < Action::HistoryPlayerFirst + static_cast<int>(history.playerCount)) {
        history.selectedSlot = history.players[action - Action::HistoryPlayerFirst].slot;
        history.roundCount = golfHistoryRowsForPlayer(history.rows, history.read.count, history.selectedSlot,
                                                      history.playerRows, GOLF_HISTORY_LIMIT);
        history.roundOffset = 0;
        screen = Screen::HistoryRounds;
      } else continue;
      repaint = true;
      forceGc = true;
    } else if (screen == Screen::HistoryRounds) {
      if (event.kind == TouchEvent::Kind::SwipeRight && history.roundOffset + 8 < history.roundCount) {
        history.roundOffset += 8;
      } else if (event.kind == TouchEvent::Kind::SwipeLeft && history.roundOffset != 0) {
        history.roundOffset = history.roundOffset >= 8 ? history.roundOffset - 8 : 0;
      } else if (event.kind == TouchEvent::Kind::Tap && action == Action::ViewBack) screen = Screen::HistoryPlayers;
      else if (event.kind == TouchEvent::Kind::Tap && action >= Action::HistoryRoundFirst &&
               action < Action::HistoryRoundFirst + static_cast<int>(history.roundCount) &&
               action < Action::HistoryRoundFirst + 8) {
        history.selectedRow = *history.playerRows[history.roundOffset + action - Action::HistoryRoundFirst];
        history.loaded = false;
        screen = Screen::HistoryRoundMenu;
      } else continue;
      repaint = true;
      forceGc = true;
    } else if (screen == Screen::HistoryRoundMenu && event.kind == TouchEvent::Kind::Tap) {
      if (action == Action::ViewBack) screen = Screen::HistoryRounds;
      else if (action == Action::HistoryDelete) screen = Screen::HistoryDeleteConfirm;
      else if (action == Action::HistoryScorecard || action == Action::HistoryReview ||
               action == Action::HistorySummary) {
        history.loaded = golfReadHistoryRound(history.selectedRow.file, history.loadedRound);
        if (history.loaded) {
          round = history.loadedRound;
          viewedPlayer = history.selectedRow.playerSlot;
          reviewedHole = 0;
          viewReturnScreen = Screen::HistoryRoundMenu;
          screen = action == Action::HistoryScorecard ? Screen::Scorecard
                   : action == Action::HistoryReview ? Screen::HoleReview : Screen::Summary;
        } else {
          viewReturnScreen = action == Action::HistorySummary ? Screen::Summary : Screen::HistoryRoundMenu;
          screen = Screen::HistoryDetailUnavailable;
        }
      } else continue;
      repaint = true;
      forceGc = true;
    } else if (screen == Screen::HistoryDetailUnavailable && event.kind == TouchEvent::Kind::Tap &&
               action == Action::ViewBack) {
      screen = Screen::HistoryRoundMenu;
      repaint = true;
      forceGc = true;
    } else if (screen == Screen::HistoryDeleteConfirm && event.kind == TouchEvent::Kind::Tap) {
      if (action == Action::ConfirmNo) screen = Screen::HistoryRoundMenu;
      else if (action == Action::ConfirmYes) {
        const size_t group = golfHistoryFileRowCount(history.rows, history.read.count, history.selectedRow.file);
        const bool removed = group > 1
                                 ? removePlayerFromRound(history.selectedRow.file, history.selectedRow.playerSlot)
                                 : removeRound(history.selectedRow.file);
        if (!removed) { screen = Screen::HistoryRoundMenu; }
        else {
          refreshHistory();
          screen = history.roundCount == 0 ? Screen::HistoryPlayers : Screen::HistoryRounds;
        }
      } else continue;
      repaint = true;
      forceGc = true;
    } else if (screen == Screen::Courses && event.kind == TouchEvent::Kind::Tap) {
      if (action == SetupBack) {
        screen = Screen::Home;
        repaint = true;
        forceGc = true;
      } else if (action >= SetupCourseFirst && action < SetupCourseFirst + GOLF_BUILT_IN_COURSE_COUNT) {
        setup.course = &GOLF_BUILT_IN_COURSES[action - SetupCourseFirst];
        for (uint8_t player = 0; player < GOLF_MAX_PLAYERS; ++player) setup.teeIndex[player] = 0;
        screen = Screen::PlayerCount;
        repaint = true;
        forceGc = true;
      }
    } else if (screen == Screen::PlayerCount && event.kind == TouchEvent::Kind::Tap) {
      if (action == SetupBack) {
        screen = Screen::Courses;
        repaint = true;
        forceGc = true;
      } else if (action == SetupCountMinus || action == SetupCountPlus) {
        setup.playerCount = stepPlayerCount(setup.playerCount, action == SetupCountMinus ? -1 : 1);
        repaint = true;
      } else if (action == SetupPrimary) {
        if (playerCountSkipsRoster(setup.playerCount)) {
          if (startSetupRound(setup, round)) {
            screen = Screen::Scoring;
            focused = GolfField::Putts;
          }
        } else screen = Screen::Roster;
        repaint = true;
        forceGc = true;
      }
    } else if (screen == Screen::Roster && event.kind == TouchEvent::Kind::Tap) {
      if (action == SetupBack) {
        screen = Screen::PlayerCount;
      } else if (action == SetupPrimary) {
        if (startSetupRound(setup, round)) {
          screen = Screen::Scoring;
          focused = GolfField::Putts;
        }
      } else if (action >= SetupPlayerFirst && action < SetupPlayerFirst + setup.playerCount) {
        setup.editPlayer = static_cast<uint8_t>(action - SetupPlayerFirst);
        screen = Screen::EditPlayer;
      }
      repaint = true;
      forceGc = true;
    } else if (screen == Screen::EditPlayer && event.kind == TouchEvent::Kind::Tap) {
      if (action == SetupBack) screen = Screen::Roster;
      else if (action == SetupEditName) {
        initializeKeyboard(keyboard, setup.playerName[setup.editPlayer]);
        screen = Screen::Keyboard;
      } else if (action == SetupEditTee) screen = Screen::TeeList;
      else continue;
      repaint = true;
      forceGc = true;
    } else if (screen == Screen::TeeList && event.kind == TouchEvent::Kind::Tap) {
      if (action == SetupBack) screen = Screen::EditPlayer;
      else if (action >= SetupTeeFirst && action < SetupTeeFirst + setup.course->teeCount) {
        setup.teeIndex[setup.editPlayer] = static_cast<uint8_t>(action - SetupTeeFirst);
        screen = Screen::EditPlayer;
      } else continue;
      repaint = true;
      forceGc = true;
    } else if (screen == Screen::Keyboard) {
      if (handleKeyboardAction(keyboard, action, event.kind)) {
        if (keyboard.finished) {
          finishKeyboard(keyboard, setup.playerName[setup.editPlayer], sizeof(setup.playerName[setup.editPlayer]));
          screen = Screen::EditPlayer;
          forceGc = true;
        }
        repaint = true;
      }
    } else if (screen == Screen::Scoring) {
      const bool forward = event.kind == TouchEvent::Kind::SwipeRight ||
                           (event.kind == TouchEvent::Kind::Tap && action == Action::Next);
      const bool backward = event.kind == TouchEvent::Kind::SwipeLeft ||
                            (event.kind == TouchEvent::Kind::Tap && action == Action::Previous);
      if (forward) {
        commitAndAdvanceGolfTurn(round);
        focused = GolfField::Putts;
        RoundStore::write(round);
        counterDirty = false;
        repaint = true;
        forceGc = true;
      } else if (backward) {
        retreatGolfTurn(round);
        focused = GolfField::Putts;
        RoundStore::write(round);
        counterDirty = false;
        repaint = true;
        forceGc = true;
      } else if ((event.kind == TouchEvent::Kind::Tap || event.kind == TouchEvent::Kind::LongPress) &&
                 action >= Action::PuttsMinus && action <= Action::Out100Plus) {
        const GolfField field = static_cast<GolfField>((action - Action::PuttsMinus) / 2);
        const bool decrement = (action - Action::PuttsMinus) % 2 == 0;
        const bool focusChanged = focused != field;
        focused = field;
        const uint16_t repeats = event.kind == TouchEvent::Kind::LongPress
                                     ? static_cast<uint16_t>(1 + (event.durationMs - GestureClassifier::LONG_PRESS_MS) / 200)
                                     : 1;
        if (changeGolfField(round, focused, decrement, repeats)) {
          counterDirty = true;
          counterChangedAt = nowMs();
          repaint = true;
        }
        if (focusChanged) {
          repaint = true;
          forceGc = true;
        }
      } else if ((event.kind == TouchEvent::Kind::Tap || event.kind == TouchEvent::Kind::LongPress) &&
                 action == Action::Menu) {
        RoundStore::write(round);
        counterDirty = false;
        viewedPlayer = round.currentPlayer;
        screen = Screen::Menu;
        repaint = true;
        forceGc = true;
      } else if (event.kind == TouchEvent::Kind::Tap && action == Action::Mark) {
        markState = initialMarkSheetState();
        screen = Screen::MarkSheet;
        repaint = true;
        forceGc = true;
      } else if (event.kind == TouchEvent::Kind::Tap && action == Action::Fairway) {
        if (toggleGolfFairway(round)) {
          counterDirty = true;
          counterChangedAt = nowMs();
          repaint = true;
        }
      }
    } else if (screen == Screen::MarkSheet && event.kind == TouchEvent::Kind::Tap) {
      if (action == Action::MarkDone || action == Action::MarkScrim) {
        RoundStore::write(round);
        counterDirty = false;
        screen = Screen::Scoring;
        repaint = true;
        forceGc = true;
      } else if (action == Action::MarkOut100 || action == Action::MarkIn100) {
        selectMarkField(markState, action == Action::MarkOut100 ? GolfField::Out100 : GolfField::In100);
        repaint = true;
      } else if (action == Action::MarkBunker) {
        if (toggleMarkBunker(round)) {
          counterDirty = true;
          counterChangedAt = nowMs();
          repaint = true;
        }
      } else if (action == Action::MarkHazardMinus || action == Action::MarkHazardPlus ||
                 action == Action::MarkObMinus || action == Action::MarkObPlus) {
        const GolfPenaltyKind kind = (action == Action::MarkHazardMinus || action == Action::MarkHazardPlus)
                                         ? GolfPenaltyKind::Hazard
                                         : GolfPenaltyKind::Ob;
        const bool increment = action == Action::MarkHazardPlus || action == Action::MarkObPlus;
        const MarkMutationResult result = changeMarkPenalty(round, markState, kind, increment);
        if (result == MarkMutationResult::Changed) {
          counterDirty = true;
          counterChangedAt = nowMs();
        }
        repaint = result != MarkMutationResult::NoChange;
      }
    } else if (screen == Screen::Menu && event.kind == TouchEvent::Kind::Tap) {
      if (action == Action::MenuBack) {
        screen = Screen::Scoring;
        repaint = true;
        forceGc = true;
      } else if (action == Action::MenuScorecard) {
        viewedPlayer = round.currentPlayer;
        viewReturnScreen = Screen::Menu;
        screen = Screen::Scorecard;
        repaint = true;
        forceGc = true;
      } else if (action == Action::MenuHoleReview) {
        viewedPlayer = round.currentPlayer;
        reviewedHole = round.currentHole;
        viewReturnScreen = Screen::Menu;
        screen = Screen::HoleReview;
        repaint = true;
        forceGc = true;
      } else if (action == Action::MenuFinish) {
        screen = Screen::ConfirmFinish;
        repaint = true;
        forceGc = true;
      } else if (action == Action::MenuAbandon) {
        screen = Screen::ConfirmAbandon;
        repaint = true;
        forceGc = true;
      }
    } else if ((screen == Screen::Scorecard || screen == Screen::Stats)) {
      if (event.kind == TouchEvent::Kind::Tap && action == Action::ViewBack) {
        screen = viewReturnScreen;
        repaint = true;
        forceGc = true;
      } else if (event.kind == TouchEvent::Kind::Tap && action == Action::PlayerChip) {
        viewedPlayer = nextEnabledPlayer(round, viewedPlayer);
        repaint = true;
        forceGc = true;
      } else if (event.kind == TouchEvent::Kind::SwipeLeft || event.kind == TouchEvent::Kind::SwipeRight) {
        screen = screen == Screen::Scorecard ? Screen::Stats : Screen::Scorecard;
        repaint = true;
      }
    } else if (screen == Screen::HoleReview) {
      const bool previous = event.kind == TouchEvent::Kind::SwipeLeft ||
                            (event.kind == TouchEvent::Kind::Tap && action == Action::Previous);
      const bool next = event.kind == TouchEvent::Kind::SwipeRight ||
                        (event.kind == TouchEvent::Kind::Tap && action == Action::Next);
      if (previous || next) {
        reviewedHole = wrapReviewHole(round, reviewedHole, previous ? -1 : 1);
        repaint = true;
      } else if (event.kind == TouchEvent::Kind::Tap && action == Action::ViewBack) {
        screen = viewReturnScreen;
        repaint = true;
        forceGc = true;
      }
    } else if (screen == Screen::ConfirmFinish && event.kind == TouchEvent::Kind::Tap) {
      if (action == Action::ConfirmNo) {
        screen = Screen::Menu;
        repaint = true;
        forceGc = true;
      } else if (action == Action::ConfirmYes) {
        viewedPlayer = round.currentPlayer;
        archivedFilename[0] = '\0';
        const RoundArchiveResult result = archiveGolfRound(round, archivedFilename, sizeof(archivedFilename));
        screen = finishDestination(result == RoundArchiveResult::Complete) == FinishDestination::Summary
                     ? Screen::Summary
                     : Screen::ArchiveError;
        repaint = true;
        forceGc = true;
      }
    } else if (screen == Screen::ConfirmAbandon && event.kind == TouchEvent::Kind::Tap) {
      if (action == Action::ConfirmNo) {
        screen = Screen::Menu;
        repaint = true;
        forceGc = true;
      } else if (action == Action::ConfirmYes) {
        RoundStore::clear();
        readHomeSummary(home);
        screen = Screen::Home;
        repaint = true;
        forceGc = true;
      }
    } else if (screen == Screen::Summary && event.kind == TouchEvent::Kind::Tap) {
      if (action == Action::ViewBack && viewReturnScreen == Screen::HistoryRoundMenu) {
        screen = Screen::HistoryRoundMenu;
        repaint = true;
        forceGc = true;
      } else if (action == Action::PlayerChip) {
        viewedPlayer = nextEnabledPlayer(round, viewedPlayer);
        repaint = true;
        forceGc = true;
      } else if (action == Action::SummaryDone) {
        readHomeSummary(home);
        screen = Screen::Home;
        repaint = true;
        forceGc = true;
      }
    } else if (screen == Screen::ArchiveError && event.kind == TouchEvent::Kind::Tap &&
               action == Action::ViewBack) {
      screen = Screen::Scoring;
      repaint = true;
      forceGc = true;
    }
    if (repaint && !paintDevice(screen, round, focused, setup, home, keyboard, forceGc, markState, paints)) return 3;
  }
}
