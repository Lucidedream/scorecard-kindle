#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "CareerStatsScreen.h"
#include "History.h"
#include "core/Course.h"
#include "core/GolfPenalty.h"
#include "store/GolfJson.h"

namespace {

constexpr const char* INDEX_HEADER =
    "date,course,holes,playerSlot,playerName,strokes,par,putts,in100,out100,hazards,obs,"
    "fairways,fairwayHoles,gir,girHoles,file\n";

GolfRound syntheticRound(const unsigned number, const uint8_t slot, const bool noParFives,
                         const bool createAroundGreenChances) {
  GolfRound round{};
  assert(applyCourse(round, GOLF_BUILT_IN_COURSES[0], GOLF_BUILT_IN_COURSES[0].tees[0].name));
  initializeGolfPlayerDefaults(round);
  snprintf(round.courseName, sizeof(round.courseName), "%s", number % 2 == 0 ? "North Links" : "Bay Club");
  for (uint8_t player = 0; player < GolfRound::MAX_PLAYERS; ++player) golfSetTee(round.players[player], "");
  snprintf(round.players[slot].name, sizeof(round.players[slot].name), "Maya");
  golfSetTee(round.players[slot], "Blue");
  for (uint8_t hole = 0; hole < round.holeCount; ++hole) {
    if (noParFives && round.par[hole] == 5) round.par[hole] = 4;
    GolfPlayerScore& score = round.players[slot].score;
    score.putts[hole] = static_cast<uint8_t>((hole + number) % 7 == 0 ? 3 : 2);
    score.in100[hole] = score.putts[hole];
    score.out100[hole] = static_cast<uint8_t>(round.par[hole] - score.putts[hole]);
    if (createAroundGreenChances && hole == 1) {
      score.out100[hole] = static_cast<uint8_t>(score.out100[hole] + 1);
      golfSetGreensideBunker(score, hole, true);
      golfSetFairwayHit(score, hole, true);
    }
  }
  // One deliberately unentered hole in later rounds exercises sparse input;
  // round zero remains a complete 18 so complete-round records populate.
  if (number != 0) {
    round.players[slot].score.putts[17] = 0;
    round.players[slot].score.in100[17] = 0;
    round.players[slot].score.out100[17] = 0;
  }
  return round;
}

void resetRoot() {
  char command[GOLF_PATH_CAPACITY + 16];
  snprintf(command, sizeof(command), "rm -rf '%s'", golfStorageRoot());
  assert(system(command) == 0 && mkdir(golfStorageRoot(), 0755) == 0);
}

// Writes N archive JSON files plus the matching hand-written index used by History.
void writeSyntheticRounds(const unsigned count, const uint8_t slot, const bool noParFives,
                          const bool createAroundGreenChances) {
  char roundsPath[GOLF_PATH_CAPACITY];
  assert(golfPath(roundsPath, sizeof(roundsPath), GOLF_ROUNDS_DIR));
  assert(mkdir(roundsPath, 0755) == 0);
  char indexPath[GOLF_PATH_CAPACITY];
  snprintf(indexPath, sizeof(indexPath), "%s/%s", roundsPath, GOLF_INDEX_FILE);
  FILE* index = fopen(indexPath, "wb");
  assert(index != nullptr && fputs(INDEX_HEADER, index) >= 0);
  for (unsigned i = 0; i < count; ++i) {
    GolfRound round = syntheticRound(i, slot, noParFives, createAroundGreenChances);
    char filename[48];
    snprintf(filename, sizeof(filename), "career-%u.json", i);
    char path[GOLF_PATH_CAPACITY];
    snprintf(path, sizeof(path), "%s/%s", roundsPath, filename);
    FILE* archived = fopen(path, "wb");
    assert(archived != nullptr && golfWriteRoundJson(archived, round, false));
    assert(fclose(archived) == 0);
    assert(fprintf(index, "2026-09-%02u,%s,18,%u,Maya,72,72,36,18,18,0,0,10,14,12,18,%s\n",
                   i + 1, round.courseName, slot, filename) > 0);
  }
  assert(fclose(index) == 0);
}

GolfCareerTally loadTally(const uint8_t slot, size_t& rowsRead) {
  HistoryRow rows[GOLF_HISTORY_LIMIT];
  const HistoryReadResult read = golfReadHistoryIndex(rows, GOLF_HISTORY_LIMIT);
  assert(read.readable);
  rowsRead = read.count;
  const HistoryRow* matches[GOLF_HISTORY_LIMIT];
  const size_t count = golfHistoryRowsForPlayer(rows, read.count, slot, matches, GOLF_HISTORY_LIMIT);
  GolfCareerTally tally{};
  for (size_t i = 0; i < count; ++i) {
    GolfRound round{};
    assert(golfReadHistoryRound(matches[i]->file, round));
    golfFoldCareerRound(round, matches[i]->playerSlot, tally);
  }
  return tally;
}

void testZeroAndOneRound() {
  resetRoot();
  char roundsPath[GOLF_PATH_CAPACITY];
  assert(golfPath(roundsPath, sizeof(roundsPath), GOLF_ROUNDS_DIR));
  assert(mkdir(roundsPath, 0755) == 0);
  size_t count = 99;
  const GolfCareerTally empty = loadTally(1, count);
  assert(count == 0);
  for (uint8_t section = 0; section < GOLF_CAREER_SECTION_COUNT; ++section)
    assert(careerStatsView(empty, section).empty);
  GolfCareerTally noParData{};
  noParData.rounds = 1;
  const CareerStatsView noShape = careerStatsView(noParData, 1);
  for (uint8_t row = 0; row < noShape.rowCount; ++row)
    assert(strstr(noShape.rows[row].value, "—") != nullptr);
  assert(strcmp(careerStatsView(noParData, 0).rows[2].value, "—") == 0);

  resetRoot();
  writeSyntheticRounds(1, 1, false, true);
  const GolfCareerTally one = loadTally(1, count);
  assert(count == 1 && one.rounds == 1 && one.lowestRound != 0 && one.fewestPutts != 0);
  const CareerStatsView hero = careerStatsView(one, 0);
  assert(!hero.empty && strcmp(hero.rows[0].value, "1") == 0);
  assert(strcmp(careerStatsView(one, 4).rows[0].value, "—") != 0);
}

void testMultipleCoursesAndMissingData() {
  resetRoot();
  writeSyntheticRounds(4, 2, false, true);
  size_t count = 0;
  const GolfCareerTally mixed = loadTally(2, count);
  assert(count == 4 && mixed.rounds == 4 && mixed.parHoles[0] != 0 && mixed.parHoles[1] != 0 &&
         mixed.parHoles[2] != 0);
  assert(mixed.scrambleChances != 0 && mixed.sandSaveChances != 0);
  assert(strstr(careerStatsView(mixed, 4).rows[0].value, "Bay Club") != nullptr ||
         strstr(careerStatsView(mixed, 4).rows[0].value, "North Links") != nullptr);

  resetRoot();
  writeSyntheticRounds(2, 0, true, false);
  const GolfCareerTally noChances = loadTally(0, count);
  const CareerStatsView byPar = careerStatsView(noChances, 2);
  assert(strcmp(byPar.rows[2].value, "—") == 0);
  const CareerStatsView green = careerStatsView(noChances, 3);
  assert(strcmp(green.rows[0].value, "—") == 0);
  assert(strcmp(green.rows[1].value, "—") == 0);
}

void testSectionWrap() {
  uint8_t page = 0;
  bool forwardSeen[GOLF_CAREER_SECTION_COUNT]{};
  for (uint8_t i = 0; i < GOLF_CAREER_SECTION_COUNT; ++i) {
    forwardSeen[page] = true;
    page = stepCareerStatsSection(page, true);
  }
  assert(page == 0);
  for (const bool seen : forwardSeen) assert(seen);
  page = stepCareerStatsSection(page, false);
  assert(page == 4);
  bool backwardSeen[GOLF_CAREER_SECTION_COUNT]{};
  for (uint8_t i = 0; i < GOLF_CAREER_SECTION_COUNT; ++i) {
    backwardSeen[page] = true;
    page = stepCareerStatsSection(page, false);
  }
  assert(page == 4);
  for (const bool seen : backwardSeen) assert(seen);
}

}  // namespace

int main() {
  testZeroAndOneRound();
  testMultipleCoursesAndMissingData();
  testSectionWrap();
  puts("M10 career stats tests passed");
  return 0;
}
