#include "HitTester.h"
#include "HoleReviewScreen.h"
#include "Keyboard.h"
#include "MarkSheet.h"
#include "PgmCanvas.h"
#include "ScorecardScreen.h"
#include "ScoringScreen.h"
#include "SetupScreens.h"
#include "SummaryScreen.h"
#include "TouchInput.h"
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
constexpr uint8_t DIM_INK = 128;
constexpr uint8_t GHOST_INK = 158;
constexpr uint64_t IDLE_WRITE_MS = 5000;

enum Action {
  Previous = 1,
  Next,
  Putts,
  In100,
  Out100,
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
};

enum class Screen { Home, History, Courses, PlayerCount, Roster, EditPlayer, TeeList, Keyboard, Scoring, Menu,
                    MarkSheet, Scorecard, Stats, HoleReview, ConfirmFinish, ConfirmAbandon, Summary,
                    ArchiveError };

PgmCanvas canvas;
HitTester hitTester;
uint8_t viewedPlayer = 0;
uint8_t reviewedHole = 0;
char archivedFilename[GOLF_ARCHIVE_NAME_CAPACITY]{};
Screen viewReturnScreen = Screen::Menu;

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
  canvas.fillRect(0, layout.holeStrip.y + layout.holeStrip.height - 2, PgmCanvas::WIDTH, 2);
  drawCentered(layout.previous, "<", TextSize::Display);
  drawCentered(layout.next, ">", TextSize::Display);
  drawCentered(layout.holeStrip, view.hole, TextSize::Display);
  hitTester.add(layout.previous, Action::Previous);
  hitTester.add(layout.next, Action::Next);
  hitTester.add({layout.previous.x + layout.previous.width, layout.holeStrip.y,
                 layout.holeStrip.width - layout.previous.width - layout.next.width,
                 layout.holeStrip.height}, Action::Menu);

  const int contextCenter = view.fairwayVisible ? (layout.fairway.x / 2) : (layout.context.width / 2);
  canvas.drawMonoText(contextCenter,
                      layout.context.y + (layout.context.height - canvas.lineHeight(TextSize::Small)) / 2,
                      view.context, TextSize::Small, TextAlign::Center, false, DIM_INK);
  if (view.fairwayVisible) {
    if (view.fairwayHit) canvas.fillRect(layout.fairway.x, layout.fairway.y, layout.fairway.width,
                                         layout.fairway.height);
    else canvas.drawRect(layout.fairway.x, layout.fairway.y, layout.fairway.width,
                         layout.fairway.height, 2);
    drawCentered(layout.fairway, "FAIRWAY", TextSize::Small, view.fairwayHit);
    hitTester.add(layout.fairway, Action::Fairway);
  }
  static constexpr char LABELS[3][16] = {"PUTTS", "INSIDE 100", "SCORE ZONE"};
  for (uint8_t index = 0; index < 3; ++index) {
    const Rect rect = layout.metrics[index];
    canvas.fillRect(38, rect.y, PgmCanvas::WIDTH - 76, 2);
    canvas.drawText(48, rect.y + 32, LABELS[index], TextSize::Small, TextAlign::Left, false, DIM_INK);
    if (view.fieldMarked[index] || (index == static_cast<uint8_t>(GolfField::In100) && view.bunkerMarked)) {
      const int flagX = 62 + canvas.measureText(LABELS[index], TextSize::Small);
      canvas.drawText(flagX, rect.y + 32, "!", TextSize::Small);
    }
    char value[8];
    snprintf(value, sizeof(value), "%u", view.values[index]);
    const TextSize size = index == static_cast<uint8_t>(focused) ? TextSize::Display : TextSize::Body;
    const uint8_t shade = view.seeded ? GHOST_INK : 0;
    canvas.drawText(PgmCanvas::WIDTH - 54, rect.y + (rect.height - canvas.lineHeight(size)) / 2,
                    value, size, TextAlign::Right, false, shade);
    hitTester.add(rect, Action::Putts + index);
  }

  canvas.fillRect(0, layout.totals.y, PgmCanvas::WIDTH, 2);
  canvas.fillRect(PgmCanvas::WIDTH / 2 - 1, layout.totals.y + 24, 2, layout.totals.height - 48);
  canvas.drawText(42, layout.totals.y + 24, "THIS HOLE", TextSize::Small, TextAlign::Left, false, DIM_INK);
  canvas.drawText(PgmCanvas::WIDTH / 2 + 42, layout.totals.y + 24, view.roundLabel, TextSize::Small,
                  TextAlign::Left, false, DIM_INK);
  char thisHole[8];
  snprintf(thisHole, sizeof(thisHole), "%u", view.thisHoleValue);
  canvas.drawText(layout.thisHole.x + layout.thisHole.width / 2, layout.totals.y + 61, thisHole,
                  TextSize::Display, TextAlign::Center, false, view.seeded ? GHOST_INK : 0);
  canvas.drawText(layout.round.x + layout.round.width / 2, layout.totals.y + 61, view.roundValue,
                  TextSize::Display, TextAlign::Center);

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
                  TextAlign::Left, false, DIM_INK);

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
    drawCentered(minus, "-", TextSize::Display, false, minusEnabled ? 0 : GHOST_INK);
    char value[8];
    snprintf(value, sizeof(value), "%u", count);
    const Rect countRect{minus.x + minus.width, row.y, plus.x - minus.x - minus.width, row.height};
    drawCentered(countRect, value, TextSize::Body);
    drawCentered(plus, "+", TextSize::Display);
  };
  drawPenaltyRow(layout.hazardRow, layout.hazardMinus, layout.hazardPlus, "Hazard  +1",
                 view.hazards, view.hazardMinusEnabled);
  drawPenaltyRow(layout.obRow, layout.obMinus, layout.obPlus, "Out of bounds  +2",
                 view.obs, view.obMinusEnabled);
  if (view.holeFull) drawCentered(layout.status, "HOLE IS FULL", TextSize::Small, false, DIM_INK);
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
  canvas.drawText(42, 38, "ROUND MENU", TextSize::Display);
  const Rect scorecard{36, 145, 1000, 220};
  const Rect review{36, 385, 1000, 220};
  const Rect finish{36, 625, 1000, 220};
  const Rect abandon{36, 865, 1000, 220};
  const Rect back{0, 1258, PgmCanvas::WIDTH, 190};
  canvas.drawRect(scorecard.x, scorecard.y, scorecard.width, scorecard.height, 2);
  canvas.drawRect(review.x, review.y, review.width, review.height, 2);
  canvas.drawRect(finish.x, finish.y, finish.width, finish.height, 2);
  canvas.drawRect(abandon.x, abandon.y, abandon.width, abandon.height, 2);
  canvas.fillRect(back.x, back.y, back.width, 2);
  drawCentered(scorecard, "View scorecard", TextSize::Body);
  drawCentered(review, "Hole review", TextSize::Body);
  drawCentered(finish, "Finish round", TextSize::Body);
  drawCentered(abandon, "Abandon round", TextSize::Body);
  drawCentered(back, "<  BACK TO SCORING", TextSize::Body);
  hitTester.add(scorecard, Action::MenuScorecard);
  hitTester.add(review, Action::MenuHoleReview);
  hitTester.add(finish, Action::MenuFinish);
  hitTester.add(abandon, Action::MenuAbandon);
  hitTester.add(back, Action::MenuBack);
}

void drawViewHeader(const char* title, const char* value, const char* player, const bool showPlayer) {
  canvas.drawText(36, 38, title, TextSize::Body);
  canvas.drawText(PgmCanvas::WIDTH - 36, 38, value, TextSize::Body, TextAlign::Right);
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
  drawViewHeader(view.header, view.roundValue, view.player, view.playerCount > 1);
  constexpr int LABEL_W = 112;
  constexpr int HOLE_W = 40;
  constexpr int OUT_W = 56;
  constexpr int IN_W = 56;
  constexpr int TOTAL_W = 68;
  constexpr int TOP = 230;
  constexpr int ROW_H = 130;
  static constexpr char ROWS[6][12] = {"PAR", "SCORE", "PUTTS", "IN 100", "ZONE", "PEN"};
  canvas.fillRect(0, TOP, PgmCanvas::WIDTH, 2);
  canvas.drawText(12, TOP + 36, "HOLE", TextSize::Small);
  int x = LABEL_W;
  for (uint8_t hole = 0; hole < 18; ++hole) {
    if (hole == 9) {
      canvas.drawMonoText(x + OUT_W / 2, TOP + 36, "OUT", TextSize::Small, TextAlign::Center);
      x += OUT_W;
    }
    char label[4];
    snprintf(label, sizeof(label), "%u", hole + 1);
    canvas.drawMonoText(x + HOLE_W / 2, TOP + 36, label, TextSize::Small, TextAlign::Center);
    x += HOLE_W;
  }
  canvas.drawMonoText(x + IN_W / 2, TOP + 36, "IN", TextSize::Small, TextAlign::Center);
  x += IN_W;
  canvas.drawMonoText(x + TOTAL_W / 2, TOP + 36, "TOT", TextSize::Small, TextAlign::Center);
  const uint8_t firstRow = view.hasPar ? 0 : 1;
  uint8_t displayRow = 0;
  for (uint8_t row = firstRow; row < 6; ++row, ++displayRow) {
    const int y = TOP + ROW_H * (displayRow + 1);
    const bool scoreRow = row == static_cast<uint8_t>(ScorecardMetric::Score);
    if (scoreRow) canvas.fillRect(0, y, PgmCanvas::WIDTH, ROW_H);
    else canvas.fillRect(0, y, PgmCanvas::WIDTH, 2);
    canvas.drawText(12, y + 38, ROWS[row], TextSize::Small, TextAlign::Left, scoreRow);
    x = LABEL_W;
    for (uint8_t hole = 0; hole < 18; ++hole) {
      if (hole == 9) {
        canvas.drawMonoText(x + OUT_W / 2, y + 38, view.out[row], TextSize::Small, TextAlign::Center, scoreRow);
        x += OUT_W;
      }
      canvas.drawMonoText(x + HOLE_W / 2, y + 38, view.cells[row][hole], TextSize::Small,
                          TextAlign::Center, scoreRow);
      x += HOLE_W;
    }
    canvas.drawMonoText(x + IN_W / 2, y + 38, view.in[row], TextSize::Small, TextAlign::Center, scoreRow);
    x += IN_W;
    canvas.drawMonoText(x + TOTAL_W / 2, y + 38, view.total[row], TextSize::Small, TextAlign::Center, scoreRow);
  }
  const int tableBottom = TOP + ROW_H * (displayRow + 1);
  const int separators[5] = {LABEL_W, LABEL_W + 9 * HOLE_W,
                             LABEL_W + 9 * HOLE_W + OUT_W,
                             LABEL_W + 18 * HOLE_W + OUT_W,
                             LABEL_W + 18 * HOLE_W + OUT_W + IN_W};
  for (const int separator : separators) canvas.fillRect(separator, TOP, 2, tableBottom - TOP);
  canvas.drawText(PgmCanvas::WIDTH / 2, 1215, "SWIPE FOR STATS", TextSize::Small, TextAlign::Center,
                  false, DIM_INK);
  drawBottomBack();
}

void drawStats(const GolfRound& round) {
  const StatsView view = statsView(round, viewedPlayer);
  hitTester.clear();
  canvas.clear();
  drawViewHeader(view.header, view.score, view.player, view.playerCount > 1);
  static constexpr char LABELS[10][20] = {"Putts", "1-putts", "3-putts", "Long game", "Short game",
                                          "Putting", "Penalties", "Fairways", "Greens", "Worst holes"};
  const char* values[10] = {view.putts, view.onePutts, view.threePutts, view.longGame, view.shortGame,
                            view.putting, view.penalties, view.fairways, view.greens, view.worst};
  const uint8_t rowCount = view.hasPar ? 10 : 9;
  int y = 230;
  for (uint8_t row = 0; row < rowCount; ++row, y += 98) {
    canvas.fillRect(36, y, PgmCanvas::WIDTH - 72, 2);
    canvas.drawText(52, y + 27, LABELS[row], TextSize::Small, TextAlign::Left, false, DIM_INK);
    canvas.drawText(PgmCanvas::WIDTH - 52, y + 27, values[row], TextSize::Small, TextAlign::Right);
  }
  canvas.drawText(PgmCanvas::WIDTH / 2, 1215, "SWIPE FOR SCORECARD", TextSize::Small,
                  TextAlign::Center, false, DIM_INK);
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
  canvas.drawMonoText(PgmCanvas::WIDTH / 2, 230, view.context, TextSize::Small, TextAlign::Center,
                      false, DIM_INK);
  canvas.drawText(PgmCanvas::WIDTH / 2, 355, view.hero, TextSize::Display, TextAlign::Center);
  static constexpr char LABELS[3][20] = {"PUTTS", "INSIDE 100", "TO SCORE ZONE"};
  const char* values[3] = {view.putts, view.in100, view.zone};
  for (uint8_t row = 0; row < 3; ++row) {
    const int y = 600 + row * 155;
    canvas.fillRect(80, y, PgmCanvas::WIDTH - 160, 2);
    canvas.drawText(100, y + 47, LABELS[row], TextSize::Small, TextAlign::Left, false, DIM_INK);
    canvas.drawText(PgmCanvas::WIDTH - 100, y + 33, values[row], TextSize::Body, TextAlign::Right);
  }
  if (view.hasPenalty) canvas.drawText(PgmCanvas::WIDTH / 2, 1080, view.penalty, TextSize::Body, TextAlign::Center);
  if (view.marks[0] != '\0') canvas.drawText(PgmCanvas::WIDTH / 2, 1165, view.marks, TextSize::Small,
                                             TextAlign::Center, false, DIM_INK);
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
  canvas.drawText(PgmCanvas::WIDTH / 2, 270, "FINISH THIS ROUND?", TextSize::Display, TextAlign::Center);
  canvas.drawText(PgmCanvas::WIDTH / 2, 410, detail, TextSize::Body, TextAlign::Center);
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
  drawViewHeader(view.header, "", view.player, view.playerCount > 1);
  canvas.drawText(PgmCanvas::WIDTH / 2, 190, view.score, TextSize::Display, TextAlign::Center);
  static constexpr char LABELS[7][18] = {"TO PAR", "PUTTS", "INSIDE 100", "LONG GAME", "PENALTIES",
                                         "FAIRWAYS", "GREENS"};
  const char* values[7] = {view.toPar, view.putts, view.in100, view.longGame, view.penalties,
                           view.fairways, view.greens};
  const uint8_t first = view.hasPar ? 0 : 1;
  uint8_t shown = 0;
  for (uint8_t index = first; index < 7; ++index, ++shown) {
    const uint8_t col = shown % 2;
    const uint8_t row = shown / 2;
    const Rect cell{36 + col * 510, 410 + row * 190, 490, 170};
    canvas.drawRect(cell.x, cell.y, cell.width, cell.height, 2);
    canvas.drawText(cell.x + 24, cell.y + 24, LABELS[index], TextSize::Small, TextAlign::Left, false, DIM_INK);
    canvas.drawText(cell.x + cell.width - 24, cell.y + 76, values[index], TextSize::Body, TextAlign::Right);
  }
  char saved[160];
  snprintf(saved, sizeof(saved), "Saved to rounds/%s.", archivedFilename);
  canvas.drawText(PgmCanvas::WIDTH / 2, 1190, saved, TextSize::Small, TextAlign::Center, false, DIM_INK);
  const Rect done{0, 1300, PgmCanvas::WIDTH, 148};
  canvas.fillRect(done.x, done.y, done.width, done.height);
  drawCentered(done, "DONE", TextSize::Body, true);
  hitTester.add(done, Action::SummaryDone);
}

void drawArchiveError() {
  hitTester.clear();
  canvas.clear();
  canvas.drawText(PgmCanvas::WIDTH / 2, 300, "COULDN'T SAVE THE ROUND", TextSize::Display, TextAlign::Center);
  canvas.drawText(PgmCanvas::WIDTH / 2, 440, "It's still open.", TextSize::Body, TextAlign::Center);
  drawBottomBack("<  BACK TO SCORING");
}

void drawAbandonConfirmation() {
  hitTester.clear();
  canvas.clear();
  canvas.drawText(PgmCanvas::WIDTH / 2, 270, "ABANDON ROUND?", TextSize::Display, TextAlign::Center);
  canvas.drawText(PgmCanvas::WIDTH / 2, 390, "This clears the current scorecard.", TextSize::Body,
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

SetupScreen setupScreen(const Screen screen) {
  switch (screen) {
    case Screen::Home: return SetupScreen::Home;
    case Screen::History: return SetupScreen::History;
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
    case Screen::History:
    case Screen::Courses:
    case Screen::PlayerCount:
    case Screen::Roster:
    case Screen::EditPlayer:
    case Screen::TeeList: drawSetupScreen(canvas, hitTester, setupScreen(screen), setup, home); break;
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
        screen = Screen::History;
        repaint = true;
        forceGc = true;
      }
    } else if (screen == Screen::History && event.kind == TouchEvent::Kind::Tap && action == SetupBack) {
      screen = Screen::Home;
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
                 action >= Action::Putts && action <= Action::Out100) {
        focused = static_cast<GolfField>(action - Action::Putts);
        const uint16_t repeats = event.kind == TouchEvent::Kind::LongPress
                                     ? static_cast<uint16_t>(1 + (event.durationMs - GestureClassifier::LONG_PRESS_MS) / 200)
                                     : 1;
        if (changeGolfField(round, focused, event.kind == TouchEvent::Kind::LongPress, repeats)) {
          counterDirty = true;
          counterChangedAt = nowMs();
          repaint = true;
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
      if (action == Action::PlayerChip) {
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
