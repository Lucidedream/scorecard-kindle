#include "SetupScreens.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "store/GolfPaths.h"
#include "UiStyle.h"

namespace {

constexpr int HEADER_HEIGHT = 150;
constexpr int FOOTER_Y = 1248;

void copyText(char* dest, const size_t capacity, const char* source) {
  if (capacity == 0) return;
  snprintf(dest, capacity, "%s", source == nullptr ? "" : source);
}

void centered(PgmCanvas& canvas, const Rect rect, const char* text, const TextSize size,
              const bool inverted = false, const uint8_t shade = 0) {
  canvas.drawText(rect.x + rect.width / 2, rect.y + (rect.height - canvas.lineHeight(size)) / 2,
                  text, size, TextAlign::Center, inverted, shade);
}

void header(PgmCanvas& canvas, HitTester& hits, const char* title) {
  const Rect strip{0, 0, PgmCanvas::WIDTH, HEADER_HEIGHT};
  canvas.fillRect(0, HEADER_HEIGHT - 2, PgmCanvas::WIDTH, 2);
  canvas.drawText(34, 45, "<", TextSize::Body);
  canvas.drawText(110, 45, title, TextSize::Body);
  hits.add(strip, SetupBack);
}

void footer(PgmCanvas& canvas, HitTester& hits, const char* primary) {
  const Rect back{0, FOOTER_Y, 360, 200};
  const Rect next{360, FOOTER_Y, PgmCanvas::WIDTH - 360, 200};
  canvas.fillRect(0, FOOTER_Y, PgmCanvas::WIDTH, 2);
  canvas.fillRect(next.x, next.y, next.width, next.height);
  centered(canvas, back, "BACK", TextSize::Body);
  centered(canvas, next, primary, TextSize::Body, true);
  hits.add(back, SetupBack);
  hits.add(next, SetupPrimary);
}

int csvFields(char* line, char* fields[], const int capacity) {
  int count = 0;
  char* read = line;
  char* write = line;
  while (*read != '\0' && count < capacity) {
    fields[count++] = write;
    bool quoted = false;
    if (*read == '"') { quoted = true; ++read; }
    while (*read != '\0') {
      if (quoted && *read == '"' && read[1] == '"') { *write++ = '"'; read += 2; continue; }
      if (quoted && *read == '"') { ++read; quoted = false; continue; }
      if (!quoted && (*read == ',' || *read == '\n' || *read == '\r')) break;
      *write++ = *read++;
    }
    const char delimiter = *read;
    *write++ = '\0';
    if (delimiter == ',') ++read;
    else while (*read == '\n' || *read == '\r') ++read;
  }
  return count;
}

void drawHome(PgmCanvas& canvas, HitTester& hits, const HomeSummary& summary) {
  constexpr int EXIT_BAR_HEIGHT = 90;
  const Rect exitBar{0, 0, PgmCanvas::WIDTH, EXIT_BAR_HEIGHT};
  const int tilesTop = EXIT_BAR_HEIGHT;
  const int tilesHeight = PgmCanvas::HEIGHT - tilesTop;
  const Rect top{0, tilesTop, PgmCanvas::WIDTH, tilesHeight / 2};
  const Rect bottom{0, top.y + top.height, PgmCanvas::WIDTH, PgmCanvas::HEIGHT - top.y - top.height};
  canvas.fillRect(0, EXIT_BAR_HEIGHT - 2, PgmCanvas::WIDTH, 2);
  canvas.drawText(PgmCanvas::WIDTH - 40, (EXIT_BAR_HEIGHT - canvas.lineHeight(TextSize::Body)) / 2, "EXIT",
                  TextSize::Body, TextAlign::Right);
  hits.add(exitBar, SetupExit);
  canvas.fillRect(top.x, top.y, top.width, top.height);
  canvas.fillRect(0, top.y + top.height, PgmCanvas::WIDTH, 3);
  constexpr int LABEL_Y = 274;
  constexpr int DETAIL_Y = 396;
  canvas.drawText(54, top.y + LABEL_Y, "NEW ROUND", TextSize::Display, TextAlign::Left, true);
  char detail[112];
  if (summary.hasLast) {
    char relative[16];
    if (summary.lastToPar == 0) snprintf(relative, sizeof(relative), "E");
    else snprintf(relative, sizeof(relative), "%+d", summary.lastToPar);
    snprintf(detail, sizeof(detail), "Last · %s · %u (%s)", summary.lastCourse, summary.lastScore, relative);
  } else copyText(detail, sizeof(detail), "No rounds yet");
  canvas.drawText(56, top.y + DETAIL_Y, detail, TextSize::Small, TextAlign::Left, false, INK_GHOST);
  canvas.drawText(54, bottom.y + LABEL_Y, "HISTORY", TextSize::Display);
  if (summary.rounds == 0) copyText(detail, sizeof(detail), "No rounds yet");
  else snprintf(detail, sizeof(detail), "%u rounds recorded", summary.rounds);
  canvas.drawText(56, bottom.y + DETAIL_Y, detail, TextSize::Small, TextAlign::Left, false, INK_DIM);
  hits.add(top, SetupNewRound);
  hits.add(bottom, SetupHistory);
}

void drawCourses(PgmCanvas& canvas, HitTester& hits, const Course* sdCourses,
                 const uint8_t sdCourseCount, const uint8_t offset) {
  header(canvas, hits, "CHOOSE COURSE");
  const int rowHeight = 250;
  const uint8_t total = static_cast<uint8_t>(GOLF_BUILT_IN_COURSE_COUNT + sdCourseCount);
  const uint8_t remaining = total > offset ? static_cast<uint8_t>(total - offset) : 0;
  const uint8_t shown = remaining < GOLF_COURSES_PER_PAGE ? remaining : GOLF_COURSES_PER_PAGE;
  for (uint8_t rowIndex = 0; rowIndex < shown; ++rowIndex) {
    const uint8_t index = static_cast<uint8_t>(offset + rowIndex);
    const Course& course = index < GOLF_BUILT_IN_COURSE_COUNT
                               ? GOLF_BUILT_IN_COURSES[index]
                               : sdCourses[index - GOLF_BUILT_IN_COURSE_COUNT];
    const Rect row{0, HEADER_HEIGHT + rowIndex * rowHeight, PgmCanvas::WIDTH, rowHeight};
    canvas.fillRect(34, row.y + row.height - 2, PgmCanvas::WIDTH - 68, 2);
    const int bodyY = row.y + (row.height - canvas.lineHeight(TextSize::Body)) / 2;
    canvas.drawText(48, bodyY - 28, course.name, TextSize::Body);
    char tees[64];
    char subtitle[96];
    courseTeeSummary(course, tees, sizeof(tees));
    snprintf(subtitle, sizeof(subtitle), "%u holes · %s", course.holeCount, tees);
    canvas.drawText(48, bodyY + 40, subtitle, TextSize::Small, TextAlign::Left, false, INK_DIM);
    char par[8];
    courseParLabel(course, par, sizeof(par));
    canvas.drawText(PgmCanvas::WIDTH - 50,
                    row.y + (row.height - canvas.lineHeight(TextSize::Display)) / 2,
                    par, TextSize::Display, TextAlign::Right);
    hits.add(row, SetupCourseFirst + index);
  }
  if (total > GOLF_COURSES_PER_PAGE)
    canvas.drawText(PgmCanvas::WIDTH - 34, 55, "SWIPE FOR MORE", TextSize::Small,
                    TextAlign::Right, false, INK_DIM);
}

void drawCount(PgmCanvas& canvas, HitTester& hits, const SetupState& setup) {
  header(canvas, hits, "PLAYERS");
  canvas.drawText(PgmCanvas::WIDTH / 2, 290, "HOW MANY PLAYING?", TextSize::Body, TextAlign::Center);
  char value[4];
  snprintf(value, sizeof(value), "%u", setup.playerCount);
  canvas.drawText(PgmCanvas::WIDTH / 2, 425, value, TextSize::Display, TextAlign::Center);
  const Rect minus{236, 660, 220, 220};
  const Rect plus{616, 660, 220, 220};
  canvas.drawRect(minus.x, minus.y, minus.width, minus.height, 4);
  canvas.drawRect(plus.x, plus.y, plus.width, plus.height, 4);
  centered(canvas, minus, "-", TextSize::Display);
  centered(canvas, plus, "+", TextSize::Display);
  hits.add(minus, SetupCountMinus);
  hits.add(plus, SetupCountPlus);
  for (uint8_t index = 0; index < GOLF_MAX_PLAYERS; ++index) {
    const int x = 422 + index * 76;
    if (index < setup.playerCount) canvas.fillRect(x, 970, 34, 34);
    else canvas.drawRect(x, 970, 34, 34, 3);
  }
  footer(canvas, hits, playerCountPrimaryLabel(setup.playerCount));
}

void drawRoster(PgmCanvas& canvas, HitTester& hits, const SetupState& setup) {
  header(canvas, hits, "PLAYERS");
  const int rowHeight = 220;
  for (uint8_t index = 0; index < setup.playerCount; ++index) {
    const Rect row{0, HEADER_HEIGHT + index * rowHeight, PgmCanvas::WIDTH, rowHeight};
    char number[4];
    snprintf(number, sizeof(number), "%u", index + 1);
    const int groupY = row.y + (row.height - 102) / 2;
    canvas.drawText(48, groupY, number, TextSize::Body);
    canvas.drawText(128, groupY, setup.playerName[index], TextSize::Body);
    const uint8_t tee = setup.teeIndex[index];
    char detail[64];
    snprintf(detail, sizeof(detail), "%s · %u yds", setup.course->tees[tee].name,
             courseTeeYards(*setup.course, tee));
    canvas.drawText(128, groupY + 67, detail, TextSize::Small, TextAlign::Left, false, INK_DIM);
    canvas.fillRect(34, row.y + row.height - 2, PgmCanvas::WIDTH - 68, 2);
    hits.add(row, SetupPlayerFirst + index);
  }
  canvas.drawText(PgmCanvas::WIDTH / 2, 1088, "Tap a player to change name or tee", TextSize::Small,
                  TextAlign::Center, false, INK_DIM);
  footer(canvas, hits, "START ROUND");
}

void drawEdit(PgmCanvas& canvas, HitTester& hits, const SetupState& setup) {
  header(canvas, hits, "EDIT PLAYER");
  const uint8_t player = setup.editPlayer;
  canvas.drawText(48, 220, setup.playerName[player], TextSize::Display);
  const Rect name{0, 390, PgmCanvas::WIDTH, 220};
  const Rect tee{0, 610, PgmCanvas::WIDTH, 220};
  const int nameY = name.y + (name.height - canvas.lineHeight(TextSize::Body)) / 2;
  const int teeY = tee.y + (tee.height - canvas.lineHeight(TextSize::Body)) / 2;
  canvas.drawText(48, nameY, "Name", TextSize::Body);
  canvas.drawText(PgmCanvas::WIDTH - 48, nameY, setup.playerName[player], TextSize::Body,
                  TextAlign::Right);
  canvas.drawText(48, teeY, "Tee", TextSize::Body);
  canvas.drawText(PgmCanvas::WIDTH - 48, teeY, setup.course->tees[setup.teeIndex[player]].name,
                  TextSize::Body, TextAlign::Right);
  canvas.fillRect(34, name.y + name.height - 2, PgmCanvas::WIDTH - 68, 2);
  canvas.fillRect(34, tee.y + tee.height - 2, PgmCanvas::WIDTH - 68, 2);
  hits.add(name, SetupEditName);
  hits.add(tee, SetupEditTee);
}

void drawTees(PgmCanvas& canvas, HitTester& hits, const SetupState& setup) {
  header(canvas, hits, "CHOOSE TEE");
  for (uint8_t index = 0; index < setup.course->teeCount; ++index) {
    const Rect row{0, HEADER_HEIGHT + index * 230, PgmCanvas::WIDTH, 230};
    const int textY = row.y + (row.height - canvas.lineHeight(TextSize::Body)) / 2;
    canvas.drawText(48, textY, setup.course->tees[index].name, TextSize::Body);
    char yards[32];
    snprintf(yards, sizeof(yards), "%u yds", courseTeeYards(*setup.course, index));
    canvas.drawText(PgmCanvas::WIDTH - 48, textY, yards, TextSize::Body, TextAlign::Right,
                    false, INK_DIM);
    canvas.fillRect(34, row.y + row.height - 2, PgmCanvas::WIDTH - 68, 2);
    hits.add(row, SetupTeeFirst + index);
  }
}

}  // namespace

void initializeSetup(SetupState& setup) {
  setup = {};
  setup.playerCount = 1;
  for (uint8_t index = 0; index < GOLF_MAX_PLAYERS; ++index) {
    copyText(setup.playerName[index], sizeof(setup.playerName[index]), GOLF_DEFAULT_PLAYER_NAMES[index]);
  }
}

uint8_t stepPlayerCount(const uint8_t count, const int direction) {
  if (direction < 0) return count > 1 ? static_cast<uint8_t>(count - 1) : 1;
  if (direction > 0) return count < GOLF_MAX_PLAYERS ? static_cast<uint8_t>(count + 1) : GOLF_MAX_PLAYERS;
  return count;
}

const char* playerCountPrimaryLabel(const uint8_t count) { return count == 1 ? "START ROUND" : "NEXT"; }
bool playerCountSkipsRoster(const uint8_t count) { return count == 1; }

void courseTeeSummary(const Course& course, char* output, const uint16_t capacity) {
  if (capacity == 0) return;
  output[0] = '\0';
  for (uint8_t index = 0; index < course.teeCount; ++index) {
    const size_t used = strlen(output);
    if (used + 1 >= capacity) break;
    snprintf(output + used, capacity - used, "%s%s", index == 0 ? "" : " / ", course.tees[index].name);
  }
}

void courseParLabel(const Course& course, char* output, const uint16_t capacity) {
  uint16_t total = 0;
  for (uint8_t hole = 0; hole < course.holeCount && hole < GOLF_MAX_HOLES; ++hole) total += course.par[hole];
  if (total == 0) copyText(output, capacity, "-");
  else snprintf(output, capacity, "%u", total);
}

uint16_t courseTeeYards(const Course& course, const uint8_t teeIndex) {
  if (teeIndex >= course.teeCount) return 0;
  uint32_t total = 0;
  for (uint8_t hole = 0; hole < course.holeCount; ++hole) total += course.tees[teeIndex].yards[hole];
  return static_cast<uint16_t>(total);
}

bool buildRoundFromSetup(const SetupState& setup, GolfRound& round) {
  if (setup.course == nullptr || setup.playerCount < 1 || setup.playerCount > GOLF_MAX_PLAYERS ||
      setup.course->teeCount == 0) return false;
  const uint8_t firstTee = setup.teeIndex[0] < setup.course->teeCount ? setup.teeIndex[0] : 0;
  if (!applyCourse(round, *setup.course, setup.course->tees[firstTee].name)) return false;
  for (uint8_t player = 0; player < GOLF_MAX_PLAYERS; ++player) {
    if (player >= setup.playerCount) { golfSetTee(round.players[player], ""); continue; }
    const uint8_t teeIndex = setup.teeIndex[player] < setup.course->teeCount ? setup.teeIndex[player] : 0;
    copyText(round.players[player].name, sizeof(round.players[player].name), setup.playerName[player]);
    golfSetTee(round.players[player], setup.course->tees[teeIndex].name);
    memcpy(round.players[player].yards, setup.course->tees[teeIndex].yards, sizeof(round.players[player].yards));
  }
  round.currentHole = 0;
  round.currentPlayer = 0;
  return true;
}

bool readHomeSummary(HomeSummary& summary) {
  summary = {};
  char relative[64];
  snprintf(relative, sizeof(relative), "%s/%s", GOLF_ROUNDS_DIR, GOLF_INDEX_FILE);
  char path[GOLF_PATH_CAPACITY];
  if (!golfPath(path, sizeof(path), relative)) return false;
  FILE* file = fopen(path, "rb");
  if (file == nullptr) return false;
  char line[1024];
  if (fgets(line, sizeof(line), file) == nullptr) { fclose(file); return false; }
  char lastFile[GOLF_ARCHIVE_NAME_CAPACITY] = "";
  while (fgets(line, sizeof(line), file) != nullptr) {
    char* fields[17]{};
    if (csvFields(line, fields, 17) != 17) continue;
    if (strcmp(lastFile, fields[16]) != 0) {
      ++summary.rounds;
      copyText(lastFile, sizeof(lastFile), fields[16]);
    }
    summary.hasLast = true;
    copyText(summary.lastCourse, sizeof(summary.lastCourse), fields[1]);
    summary.lastScore = static_cast<uint16_t>(strtoul(fields[5], nullptr, 10));
    const long par = strtol(fields[6], nullptr, 10);
    summary.lastToPar = static_cast<int16_t>(static_cast<long>(summary.lastScore) - par);
  }
  fclose(file);
  return summary.hasLast;
}

void drawSetupScreen(PgmCanvas& canvas, HitTester& hits, const SetupScreen screen, const SetupState& setup,
                     const HomeSummary& summary, const Course* sdCourses,
                     const uint8_t sdCourseCount, const uint8_t courseOffset) {
  canvas.clear();
  hits.clear();
  switch (screen) {
    case SetupScreen::Home: drawHome(canvas, hits, summary); break;
    case SetupScreen::Courses: drawCourses(canvas, hits, sdCourses, sdCourseCount, courseOffset); break;
    case SetupScreen::PlayerCount: drawCount(canvas, hits, setup); break;
    case SetupScreen::Roster: drawRoster(canvas, hits, setup); break;
    case SetupScreen::EditPlayer: drawEdit(canvas, hits, setup); break;
    case SetupScreen::TeeList: drawTees(canvas, hits, setup); break;
  }
}
