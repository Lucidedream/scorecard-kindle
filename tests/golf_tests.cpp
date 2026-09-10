#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "core/Course.h"
#include "core/GolfCareerStats.h"
#include "core/GolfPenalty.h"
#include "core/GolfRules.h"
#include "core/GolfStats.h"
#include "store/GolfJson.h"
#include "store/GolfPaths.h"
#include "store/RoundArchive.h"
#include "store/RoundStore.h"

namespace {

int tests;
GolfRound round;
GolfRound loaded;

void resetRound() {
  round = {};
  assert(applyCourse(round, GOLF_BUILT_IN_COURSES[0], "Blue"));
  round.dateYmd = static_cast<uint16_t>((26U << 9) | (9U << 5) | 10U);
}

void enter(const uint8_t hole, const uint8_t in100, const uint8_t out100, const uint8_t putts) {
  round.players[0].score.in100[hole] = in100;
  round.players[0].score.out100[hole] = out100;
  round.players[0].score.putts[hole] = putts;
}

void testModelAndCourse() {
  resetRound();
  assert(sizeof(GolfRound) == 970);
  assert(strcmp(round.courseName, "Sanyang Golf Club") == 0);
  assert(strcmp(round.players[0].tee, "Blue") == 0);
  assert(round.players[0].yards[0] == 325);
  assert(golfEnabledPlayerCount(round) == 1);
  golfSetTee(round.players[2], "Red");
  round.players[2].score.in100[0] = 3;
  assert(golfDisablePlayer(round, 2));
  assert(!golfPlayerIsEnabled(round.players[2]));
  ++tests;
}

void testRules() {
  resetRound();
  GolfPlayerScore& score = round.players[0].score;
  score.putts[0] = 2;
  score.in100[0] = 2;
  GolfMutationResult result = incrementGolfCounter(score, 0, GolfField::Putts);
  assert(result.changed && result.carriedIn100 && score.in100[0] == 3);
  result = decrementGolfCounter(score, 0, GolfField::In100);
  assert(result.changed && result.loweredPutts && score.putts[0] == 2);
  score = {};
  assert(seedGolfHoleAtPar(score, 0, 4));
  assert(score.putts[0] == 2 && score.in100[0] == 2 && score.out100[0] == 2);
  assert(nextGolfField(GolfField::Out100) == GolfField::Putts);
  ++tests;
}

void testTurns() {
  resetRound();
  golfSetTee(round.players[2], "White");
  round.currentHole = 4;
  round.currentPlayer = 0;
  assert(advanceGolfTurn(round) && round.currentPlayer == 2 && round.currentHole == 4);
  assert(advanceGolfTurn(round) && round.currentPlayer == 0 && round.currentHole == 5);
  assert(retreatGolfTurn(round) && round.currentPlayer == 2 && round.currentHole == 4);
  round.currentHole = 17;
  assert(golfIsFinalCommit(round));
  ++tests;
}

void testPenalty() {
  resetRound();
  GolfPlayerScore& score = round.players[0].score;
  assert(golfAppendPenalty(score, 0, GolfField::Out100, GolfPenaltyKind::Hazard) ==
         GolfPenaltyMutationStatus::Changed);
  assert(golfAppendPenalty(score, 0, GolfField::In100, GolfPenaltyKind::Ob) == GolfPenaltyMutationStatus::Changed);
  assert(golfPenaltyStrokesForHole(score, 0) == 3);
  assert(golfRemoveLatestPenalty(score, 0, GolfField::Out100) == GolfPenaltyMutationStatus::Changed);
  assert(golfPenaltyStrokesForHole(score, 0) == 2);
  golfSetFairwayHit(score, 8, true);
  golfSetGreensideBunker(score, 8, true);
  assert(golfFairwayHit(score, 8) && golfGreensideBunker(score, 8));
  assert(!golfFairwayHit(score, 18));
  ++tests;
}

void testStatsEnteredOnly() {
  resetRound();
  enter(0, 2, 3, 1);
  round.players[0].score.putts[1] = 9;
  enter(2, 4, 3, 3);
  assert(golfScore(round, round.players[0].score) == 12);
  assert(golfThru(round, round.players[0].score) == 2);
  assert(golfPuttsTotal(round, round.players[0].score) == 4);
  assert(golfIn100Total(round, round.players[0].score) == 6);
  assert(golfLongTotal(round, round.players[0].score) == 6);
  assert(golfOnePutts(round, round.players[0].score) == 1);
  assert(golfThreePutts(round, round.players[0].score) == 1);
  ++tests;
}

void testRegulationAndWorst() {
  resetRound();
  enter(0, 2, 2, 2);
  enter(1, 3, 3, 1);
  golfSetFairwayHit(round.players[0].score, 0, true);
  golfSetGreensideBunker(round.players[0].score, 0, true);
  assert(golfGreenInRegulation(round, round.players[0].score, 0));
  assert(golfGreensInRegulation(round, round.players[0].score) == 1);
  assert(golfGreensEligible(round, round.players[0].score) == 2);
  assert(golfFairwaysHit(round, round.players[0].score) == 1);
  assert(golfFairwaysEligible(round, round.players[0].score) == 2);
  assert(golfSandSaves(round, round.players[0].score) == 1);
  GolfWorstHole worst[2]{};
  assert(golfWorstHoles(round, round.players[0].score, worst, 2) == 2);
  assert(worst[0].hole == 1);
  ++tests;
}

void testCareerStats() {
  resetRound();
  for (uint8_t hole = 0; hole < 18; ++hole) {
    round.par[hole] = 4;
    enter(hole, 2, 2, 2);
  }
  GolfCareerTally tally{};
  golfFoldCareerRound(round, 0, tally);
  assert(tally.rounds == 1 && tally.parRounds == 1);
  assert(tally.lowestRound == 72 && tally.fewestPutts == 36);
  assert(tally.mostPars == 18 && tally.longestBogeyFreeRun == 18);
  ++tests;
}

void testRoundTrip() {
  resetRound();
  golfSetTee(round.players[1], "White");
  enter(0, 2, 2, 2);
  round.players[1].score.in100[1] = 2;
  round.players[1].score.out100[1] = 3;
  golfSetFairwayHit(round.players[1].score, 1, true);
  golfSetGreensideBunker(round.players[1].score, 1, true);
  assert(RoundStore::write(round));
  loaded = {};
  const GolfJsonResult result = RoundStore::read(loaded);
  assert(result.status == GolfJsonStatus::Ok);
  assert(memcmp(&round, &loaded, sizeof(round)) == 0);
  ++tests;
}

void testValidationRepairs() {
  resetRound();
  for (GolfPlayer& player : round.players) golfSetTee(player, "");
  round.currentHole = 99;
  round.currentPlayer = 3;
  assert(RoundStore::write(round));
  GolfJsonResult result = RoundStore::read(loaded);
  assert(result.status == GolfJsonStatus::Ok && result.validation.repaired());
  assert(strcmp(loaded.players[0].tee, "Blue") == 0);
  assert(loaded.currentHole == 0 && loaded.currentPlayer == 0);

  resetRound();
  round.players[0].score.putts[0] = 8;
  round.players[0].score.in100[0] = 2;
  round.players[0].score.out100[0] = 0;
  round.players[0].score.penaltyCount[0] = 1;
  round.players[0].score.penaltyEvents[0][0] =
      golfPackPenaltyEvent(GolfField::Out100, GolfPenaltyKind::Hazard);
  assert(RoundStore::write(round));
  result = RoundStore::read(loaded);
  assert(result.status == GolfJsonStatus::Ok && result.validation.repaired());
  assert(loaded.players[0].score.putts[0] == 2);
  assert(loaded.players[0].score.penaltyCount[0] == 0);
  ++tests;
}

void testRejectedJson() {
  const char legacy[] = "{\"v\":5}";
  assert(golfReadRoundJson(legacy, sizeof(legacy) - 1, true, loaded).status == GolfJsonStatus::RejectedVersion);
  const char bad[] = "{nope";
  assert(golfReadRoundJson(bad, sizeof(bad) - 1, true, loaded).status == GolfJsonStatus::InvalidJson);
  const char committed[] = "{\"v\":garbage,\"archivedAs\":\"round-0001-course.json\"}";
  assert(golfReadRoundJson(committed, sizeof(committed) - 1, true, loaded).status == GolfJsonStatus::Archived);
  ++tests;
}

void testArrayLengthMismatch() {
  resetRound();
  assert(RoundStore::write(round));
  char path[GOLF_PATH_CAPACITY];
  assert(golfPath(path, sizeof(path), GOLF_STATE_FILE));
  FILE* file = fopen(path, "rb");
  assert(file != nullptr);
  assert(fseek(file, 0, SEEK_END) == 0);
  const long length = ftell(file);
  assert(length > 0 && fseek(file, 0, SEEK_SET) == 0);
  char* json = static_cast<char*>(malloc(static_cast<size_t>(length) + 1));
  assert(json != nullptr && fread(json, 1, static_cast<size_t>(length), file) == static_cast<size_t>(length));
  fclose(file);
  json[length] = '\0';
  char* par = strstr(json, "\"par\":[4,5,");
  assert(par != nullptr);
  par += strlen("\"par\":[4,");
  memmove(par, par + 2, static_cast<size_t>(json + length - par - 1));
  const GolfJsonResult result = golfReadRoundJson(json, static_cast<size_t>(length - 2), true, loaded);
  free(json);
  assert(result.status != GolfJsonStatus::Ok);
  ++tests;
}

void testSlug() {
  char slug[41];
  golfSlug("  Pebble  Beach!! ", slug, sizeof(slug));
  assert(strcmp(slug, "pebble-beach") == 0);
  golfSlug("Crème Brûlée", slug, sizeof(slug));
  assert(strcmp(slug, "cr-me-br-l-e") == 0);
  golfSlug("!!!", slug, sizeof(slug));
  assert(strcmp(slug, "course") == 0);
  ++tests;
}

int csvDataRows(const char* path) {
  FILE* file = fopen(path, "rb");
  assert(file != nullptr);
  int lines = 0;
  int byte = 0;
  while ((byte = fgetc(file)) != EOF) if (byte == '\n') ++lines;
  fclose(file);
  return lines - 1;
}

void testArchiveAndSequence() {
  resetRound();
  golfSetTee(round.players[1], "White");
  enter(0, 2, 2, 2);
  round.players[1].score.in100[0] = 2;
  round.players[1].score.out100[0] = 3;
  assert(RoundStore::write(round));
  char first[GOLF_ARCHIVE_NAME_CAPACITY];
  assert(archiveGolfRound(round, first, sizeof(first)) == RoundArchiveResult::Complete);
  assert(strncmp(first, "round-0001-", 11) == 0);
  char state[GOLF_PATH_CAPACITY];
  assert(golfPath(state, sizeof(state), GOLF_STATE_FILE) && access(state, F_OK) != 0);
  char index[GOLF_PATH_CAPACITY];
  assert(golfPath(index, sizeof(index), "rounds/index.csv") && csvDataRows(index) == 2);

  assert(RoundStore::write(round));
  char second[GOLF_ARCHIVE_NAME_CAPACITY];
  assert(archiveGolfRound(round, second, sizeof(second)) == RoundArchiveResult::Complete);
  assert(strncmp(second, "round-0002-", 11) == 0 && strcmp(first, second) != 0);
  assert(csvDataRows(index) == 4);
  ++tests;
}

void testArchiveMarkerPrecedence() {
  assert(RoundStore::writeArchiveMarker("round-0099-course.json"));
  char committed[GOLF_ARCHIVE_NAME_CAPACITY];
  assert(archiveGolfRound(round, committed, sizeof(committed)) == RoundArchiveResult::Complete);
  assert(strcmp(committed, "round-0099-course.json") == 0);
  char state[GOLF_PATH_CAPACITY];
  assert(golfPath(state, sizeof(state), GOLF_STATE_FILE) && access(state, F_OK) != 0);
  ++tests;
}

}  // namespace

int main() {
  testModelAndCourse();
  testRules();
  testTurns();
  testPenalty();
  testStatsEnteredOnly();
  testRegulationAndWorst();
  testCareerStats();
  testRoundTrip();
  testValidationRepairs();
  testRejectedJson();
  testArrayLengthMismatch();
  testSlug();
  testArchiveAndSequence();
  testArchiveMarkerPrecedence();
  printf("Golf tests passed: %d\n", tests);
  return 0;
}
