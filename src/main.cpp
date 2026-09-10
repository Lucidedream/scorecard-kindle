#include "HitTester.h"
#include "Keyboard.h"
#include "PgmCanvas.h"
#include "ScoringScreen.h"
#include "SetupScreens.h"
#include "TouchInput.h"
#include "core/Course.h"
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
  MenuAbandon,
  MenuBack,
  ConfirmNo,
  ConfirmYes,
};

enum class Screen { Home, History, Courses, PlayerCount, Roster, EditPlayer, TeeList, Keyboard, Scoring, Menu,
                    ConfirmAbandon };

PgmCanvas canvas;
HitTester hitTester;

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

  canvas.drawMonoText(layout.context.x + layout.context.width / 2,
                      layout.context.y + (layout.context.height - canvas.lineHeight(TextSize::Small)) / 2,
                      view.context, TextSize::Small, TextAlign::Center, false, DIM_INK);
  static constexpr char LABELS[3][16] = {"PUTTS", "INSIDE 100", "SCORE ZONE"};
  for (uint8_t index = 0; index < 3; ++index) {
    const Rect rect = layout.metrics[index];
    canvas.fillRect(38, rect.y, PgmCanvas::WIDTH - 76, 2);
    canvas.drawText(48, rect.y + 32, LABELS[index], TextSize::Small, TextAlign::Left, false, DIM_INK);
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
  drawCentered(layout.mark, "MARK", TextSize::Body, false, GHOST_INK);
  drawCentered(layout.nextHole, "NEXT HOLE >", TextSize::Body, true);
  hitTester.add(layout.menu, Action::Menu);
  hitTester.add(layout.mark, Action::Mark);
  hitTester.add(layout.nextHole, Action::Next);
}

void drawMenu() {
  hitTester.clear();
  canvas.clear();
  canvas.drawText(42, 38, "ROUND MENU", TextSize::Display);
  const Rect scorecard{36, 180, 1000, 220};
  const Rect abandon{36, 430, 1000, 220};
  const Rect back{36, 680, 1000, 220};
  canvas.drawRect(scorecard.x, scorecard.y, scorecard.width, scorecard.height, 2);
  canvas.drawRect(abandon.x, abandon.y, abandon.width, abandon.height, 2);
  canvas.drawRect(back.x, back.y, back.width, back.height, 2);
  drawCentered(scorecard, "View scorecard", TextSize::Body, false, GHOST_INK);
  drawCentered(abandon, "Abandon round", TextSize::Body);
  drawCentered(back, "Back to scoring", TextSize::Body);
  hitTester.add(scorecard, Action::MenuScorecard);
  hitTester.add(abandon, Action::MenuAbandon);
  hitTester.add(back, Action::MenuBack);
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
           const bool showOnDevice, const bool fullRefresh) {
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
    case Screen::Menu: drawMenu(); break;
    case Screen::ConfirmAbandon: drawAbandonConfirmation(); break;
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
                 unsigned int& paints) {
  if (!paint(SCREEN_PATH, screen, round, focused, setup, home, keyboard, true, fullRefresh)) return false;
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
    return paint(argv[2], Screen::Scoring, round, GolfField::Putts, setup, home, keyboard, false, true) ? 0 : 1;
  }
  if (argc == 4 && strcmp(argv[1], "--render") == 0) {
    GolfRound round{};
    SetupState setup{};
    HomeSummary home{};
    KeyboardState keyboard{};
    initializeSetup(setup);
    initializeKeyboard(keyboard, "Noah");
    makeGoldenRound(round);
    Screen screen = Screen::Home;
    if (strcmp(argv[2], "keyboard") == 0) screen = Screen::Keyboard;
    else if (strcmp(argv[2], "scoring") == 0) screen = Screen::Scoring;
    else if (strcmp(argv[2], "home") != 0) return 2;
    return paint(argv[3], screen, round, GolfField::Putts, setup, home, keyboard, false, true) ? 0 : 1;
  }

  GolfRound round{};
  const bool hasRound = RoundStore::read(round).status == GolfJsonStatus::Ok;
  SetupState setup{};
  HomeSummary home{};
  KeyboardState keyboard{};
  initializeSetup(setup);
  readHomeSummary(home);
  GolfField focused = GolfField::Putts;
  Screen screen = hasRound ? Screen::Scoring : Screen::Home;
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
  if (!paintDevice(screen, round, focused, setup, home, keyboard, true, paints)) return 3;
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
      } else if (event.kind == TouchEvent::Kind::Tap && action == Action::Menu) {
        RoundStore::write(round);
        counterDirty = false;
        screen = Screen::Menu;
        repaint = true;
        forceGc = true;
      }
    } else if (screen == Screen::Menu && event.kind == TouchEvent::Kind::Tap) {
      if (action == Action::MenuBack) {
        screen = Screen::Scoring;
        repaint = true;
        forceGc = true;
      } else if (action == Action::MenuAbandon) {
        screen = Screen::ConfirmAbandon;
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
    }
    if (repaint && !paintDevice(screen, round, focused, setup, home, keyboard, forceGc, paints)) return 3;
  }
}
