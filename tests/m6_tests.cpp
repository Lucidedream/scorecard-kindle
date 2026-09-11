#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "History.h"
#include "core/Course.h"
#include "store/RoundArchive.h"
#include "store/RoundStore.h"

namespace {

GolfRound makeRound(const char* course = "History Links") {
  GolfRound round{};
  assert(applyCourse(round, GOLF_BUILT_IN_COURSES[0], GOLF_BUILT_IN_COURSES[0].tees[0].name));
  snprintf(round.courseName, sizeof(round.courseName), "%s", course);
  initializeGolfPlayerDefaults(round);
  golfSetTee(round.players[0], GOLF_BUILT_IN_COURSES[0].tees[0].name);
  round.dateYmd = static_cast<uint16_t>((26U << 9) | (9U << 5) | 10U);
  for (uint8_t h = 0; h < 18; ++h) {
    round.players[0].yards[h] = GOLF_BUILT_IN_COURSES[0].tees[0].yards[h];
    round.players[0].score.putts[h] = 2;
    round.players[0].score.in100[h] = 3;
    round.players[0].score.out100[h] = 1;
  }
  round.currentHole = 17;
  return round;
}

void archive(GolfRound& round, char* file) {
  assert(RoundStore::write(round));
  assert(archiveGolfRound(round, file, GOLF_ARCHIVE_NAME_CAPACITY) == RoundArchiveResult::Complete);
}

char* readFile(const char* relative) {
  char path[GOLF_PATH_CAPACITY];
  assert(golfPath(path, sizeof(path), relative));
  FILE* input = fopen(path, "rb");
  assert(input != nullptr && fseek(input, 0, SEEK_END) == 0);
  const long size = ftell(input);
  assert(size >= 0 && fseek(input, 0, SEEK_SET) == 0);
  char* result = static_cast<char*>(malloc(static_cast<size_t>(size) + 1));
  assert(result != nullptr && fread(result, 1, static_cast<size_t>(size), input) == static_cast<size_t>(size));
  result[size] = '\0';
  fclose(input);
  return result;
}

void testReaderFixture() {
  char rounds[GOLF_PATH_CAPACITY];
  assert(golfPath(rounds, sizeof(rounds), GOLF_ROUNDS_DIR));
  assert(mkdir(rounds, 0755) == 0);
  char index[GOLF_PATH_CAPACITY];
  snprintf(index, sizeof(index), "%s/%s", rounds, GOLF_INDEX_FILE);
  FILE* out = fopen(index, "wb");
  assert(out != nullptr);
  fputs("date,course,holes,playerSlot,playerName,strokes,par,putts,in100,out100,hazards,obs,fairways,fairwayHoles,gir,girHoles,file\n", out);
  fputs("2026-09-01,\"Links, East\",18,0,\"Noah \"\"N\"\"\",88,72,32,50,38,0,0,8,14,7,18,r1.json\n", out);
  fputs("null,West,18,1,Maya,90,72,34,52,38,0,0,7,14,6,18,r2.json\n", out);
  fputs("malformed,row\n", out);
  fputs("2026-09-03,North,18,0,Noah,86,72,30,48,38,0,0,9,14,8,18,r3.json\n", out);
  fclose(out);
  HistoryRow rows[50];
  HistoryReadResult result = golfReadHistoryIndex(rows, 50);
  assert(result.readable && result.count == 3 && !result.olderRoundsExist);
  assert(strcmp(rows[0].course, "Links, East") == 0 && strcmp(rows[0].playerName, "Noah \"N\"") == 0);
  assert(rows[1].date[0] == '\0');
  HistoryPlayer players[4];
  assert(golfHistoryPlayers(rows, result.count, players, 4) == 2);
  assert(players[0].slot == 0 && players[0].roundCount == 2);
  const HistoryRow* newest[3];
  assert(golfHistoryRowsForPlayer(rows, result.count, 0, newest, 3) == 2);
  assert(strcmp(newest[0]->file, "r3.json") == 0);

  out = fopen(index, "ab");
  assert(out != nullptr);
  for (unsigned i = 0; i < 55; ++i)
    fprintf(out, "2026-09-04,Course,18,0,Noah,%u,72,30,48,38,0,0,9,14,8,18,x%u.json\n", 80 + i, i);
  fclose(out);
  result = golfReadHistoryIndex(rows, 50);
  assert(result.count == 50 && result.olderRoundsExist && strcmp(rows[49].file, "x54.json") == 0);
}

void resetRoot() {
  char command[GOLF_PATH_CAPACITY + 16];
  snprintf(command, sizeof(command), "rm -rf '%s'", golfStorageRoot());
  assert(system(command) == 0 && mkdir(golfStorageRoot(), 0755) == 0);
}

void testSummaryAndDeletes() {
  resetRoot();
  GolfRound one = makeRound("Solo Course");
  char solo[GOLF_ARCHIVE_NAME_CAPACITY];
  archive(one, solo);
  GolfRound shared = makeRound("Shared Course");
  for (uint8_t slot = 1; slot < 3; ++slot) {
    golfSetTee(shared.players[slot], "Blue");
    for (uint8_t h = 0; h < 18; ++h) {
      shared.players[slot].yards[h] = shared.players[0].yards[h];
      shared.players[slot].score.putts[h] = static_cast<uint8_t>(1 + slot);
      shared.players[slot].score.in100[h] = 3;
      shared.players[slot].score.out100[h] = 1;
    }
  }
  char sharedFile[GOLF_ARCHIVE_NAME_CAPACITY];
  archive(shared, sharedFile);
  HistoryRow rows[50];
  HistoryReadResult result = golfReadHistoryIndex(rows, 50);
  assert(result.count == 4 && golfHistoryFileRowCount(rows, result.count, sharedFile) == 3);
  SummaryView csvView = golfHistorySummaryView(rows[0]);
  SummaryView fullView = summaryView(one, 0);
  assert(strcmp(csvView.score, fullView.score) == 0 && strcmp(csvView.toPar, fullView.toPar) == 0);
  assert(strcmp(csvView.putts, fullView.putts) == 0 && strcmp(csvView.longGame, fullView.longGame) == 0);

  assert(removeRound(solo));
  result = golfReadHistoryIndex(rows, 50);
  assert(result.count == 3 && golfHistoryFileRowCount(rows, result.count, solo) == 0);
  char archivePath[GOLF_PATH_CAPACITY];
  snprintf(archivePath, sizeof(archivePath), "%s/%s/%s", golfStorageRoot(), GOLF_ROUNDS_DIR, solo);
  assert(access(archivePath, F_OK) != 0);

  char indexRelative[GOLF_PATH_CAPACITY];
  snprintf(indexRelative, sizeof(indexRelative), "%s/%s", GOLF_ROUNDS_DIR, GOLF_INDEX_FILE);
  char* before = readFile(indexRelative);
  assert(setenv("GOLF_ARCHIVE_FAIL_BEFORE_INDEX_RENAME", "1", 1) == 0);
  assert(!removePlayerFromRound(sharedFile, 1));
  unsetenv("GOLF_ARCHIVE_FAIL_BEFORE_INDEX_RENAME");
  char* after = readFile(indexRelative);
  assert(strcmp(before, after) == 0);
  free(before);
  free(after);

  assert(removePlayerFromRound(sharedFile, 1));
  result = golfReadHistoryIndex(rows, 50);
  assert(result.count == 2 && golfHistoryFileRowCount(rows, result.count, sharedFile) == 2);
  GolfRound loaded{};
  assert(golfReadHistoryRound(sharedFile, loaded));
  assert(!golfPlayerIsEnabled(loaded.players[1]));
  assert(golfPlayerIsEnabled(loaded.players[0]) && golfPlayerIsEnabled(loaded.players[2]));
  assert(loaded.players[1].score.putts[0] == 0);
}

}  // namespace

int main() {
  testReaderFixture();
  testSummaryAndDeletes();
  puts("M6 history tests passed");
  return 0;
}
