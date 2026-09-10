#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "HoleReviewScreen.h"
#include "ScorecardScreen.h"
#include "SummaryScreen.h"
#include "core/Course.h"
#include "core/GolfPenalty.h"
#include "store/GolfPaths.h"
#include "store/RoundArchive.h"
#include "store/RoundStore.h"

namespace {

GolfRound makeRound() {
  GolfRound round{};
  assert(applyCourse(round, GOLF_BUILT_IN_COURSES[0], GOLF_BUILT_IN_COURSES[0].tees[0].name));
  initializeGolfPlayerDefaults(round);
  golfSetTee(round.players[0], GOLF_BUILT_IN_COURSES[0].tees[0].name);
  for (uint8_t hole = 0; hole < round.holeCount; ++hole)
    round.players[0].yards[hole] = GOLF_BUILT_IN_COURSES[0].tees[0].yards[hole];
  round.dateYmd = static_cast<uint16_t>((26U << 9) | (9U << 5) | 10U);
  return round;
}

void enter(GolfRound& round, const uint8_t hole, const uint8_t putts, const uint8_t in100,
           const uint8_t zone) {
  round.players[0].score.putts[hole] = putts;
  round.players[0].score.in100[hole] = in100;
  round.players[0].score.out100[hole] = zone;
}

void testScorecardAndStats() {
  GolfRound round = makeRound();
  enter(round, 0, 2, 3, 2);
  enter(round, 8, 1, 2, 2);
  enter(round, 9, 3, 4, 3);
  assert(golfAppendPenalty(round.players[0].score, 0, GolfField::Out100, GolfPenaltyKind::Hazard) ==
         GolfPenaltyMutationStatus::Changed);
  ScorecardView card = scorecardView(round, 0);
  assert(card.hasPar);
  assert(strcmp(card.cells[static_cast<uint8_t>(ScorecardMetric::Score)][0], "7") == 0);
  assert(strcmp(card.cells[static_cast<uint8_t>(ScorecardMetric::Score)][1], "-") == 0);
  assert(strcmp(card.out[static_cast<uint8_t>(ScorecardMetric::Score)], "11") == 0);
  assert(strcmp(card.in[static_cast<uint8_t>(ScorecardMetric::Score)], "7") == 0);
  assert(strcmp(card.total[static_cast<uint8_t>(ScorecardMetric::Score)], "18") == 0);
  assert(strcmp(card.cells[static_cast<uint8_t>(ScorecardMetric::Pen)][0], "1") == 0);
  assert(strcmp(card.cells[static_cast<uint8_t>(ScorecardMetric::Pen)][8], "-") == 0);
  StatsView stats = statsView(round, 0);
  assert(strstr(stats.score, "18") != nullptr && strstr(stats.score, "+") != nullptr);
  assert(strcmp(stats.fairways, "0/3") == 0);
  assert(strcmp(stats.greens, "0/3") == 0);
  assert(strstr(stats.penalties, "HZD 1") != nullptr && strstr(stats.penalties, "OB 0") != nullptr);
  assert(strstr(stats.worst, "H10 +3") != nullptr);

  golfSetTee(round.players[2], "White");
  assert(nextEnabledPlayer(round, 0) == 2 && nextEnabledPlayer(round, 2) == 0);
  assert(scorecardView(round, 2).playerSlot == 2 && scorecardView(round, 2).playerCount == 2);

  memset(round.par, 0, sizeof(round.par));
  card = scorecardView(round, 0);
  stats = statsView(round, 0);
  assert(!card.hasPar && strchr(card.roundValue, '+') == nullptr && strchr(card.roundValue, 'E') == nullptr);
  assert(!stats.hasPar && stats.worst[0] == '\0');
}

void testReview() {
  GolfRound round = makeRound();
  enter(round, 6, 2, 5, 2);
  golfSetFairwayHit(round.players[0].score, 6, true);
  HoleReviewView view = holeReviewView(round, 0, 6);
  assert(strcmp(view.hero, "7   +3") == 0);
  assert(!view.hasPenalty && view.penalty[0] == '\0');
  assert(strstr(view.marks, "FAIRWAY") != nullptr);
  assert(golfAppendPenalty(round.players[0].score, 6, GolfField::In100, GolfPenaltyKind::Ob) ==
         GolfPenaltyMutationStatus::Changed);
  view = holeReviewView(round, 0, 6);
  assert(strcmp(view.hero, "10   +6") == 0);
  assert(view.hasPenalty && strstr(view.penalty, "PENALTY +2") != nullptr && strstr(view.penalty, "OB 1") != nullptr);
  assert(wrapReviewHole(round, 0, -1) == 17);
  assert(wrapReviewHole(round, 17, 1) == 0);
}

void testSummary() {
  GolfRound round = makeRound();
  for (uint8_t hole = 0; hole < 18; ++hole) enter(round, hole, 2, 4, 2);
  SummaryView view = summaryView(round, 0);
  assert(view.hasPar && strcmp(view.score, "108") == 0 && strcmp(view.putts, "36") == 0);
  assert(strcmp(view.in100, "72") == 0 && strcmp(view.longGame, "36") == 0);
  memset(round.par, 0, sizeof(round.par));
  view = summaryView(round, 0);
  assert(!view.hasPar && view.toPar[0] == '\0');
}

void testFinishWalk() {
  GolfRound round = makeRound();
  for (uint8_t hole = 0; hole < 18; ++hole) enter(round, hole, 2, 2, static_cast<uint8_t>(round.par[hole] - 2));
  round.currentHole = 17;
  const SummaryView expected = summaryView(round, round.currentPlayer);
  assert(RoundStore::write(round));
  char filename[GOLF_ARCHIVE_NAME_CAPACITY]{};
  const RoundArchiveResult result = archiveGolfRound(round, filename, sizeof(filename));
  assert(result == RoundArchiveResult::Complete);
  assert(finishDestination(result == RoundArchiveResult::Complete) == FinishDestination::Summary);
  assert(filename[0] != '\0');
  GolfRound loaded{};
  assert(RoundStore::read(loaded).status == GolfJsonStatus::IoError);
  const SummaryView actual = summaryView(round, round.currentPlayer);
  assert(strcmp(actual.score, expected.score) == 0 && strcmp(actual.toPar, expected.toPar) == 0);

  char badRoot[GOLF_PATH_CAPACITY];
  snprintf(badRoot, sizeof(badRoot), "%s/forced-failure", golfStorageRoot());
  assert(mkdir(badRoot, 0755) == 0);
  assert(setenv("SCORECARD_DIR", badRoot, 1) == 0);
  assert(RoundStore::write(round));
  char roundsPath[GOLF_PATH_CAPACITY];
  snprintf(roundsPath, sizeof(roundsPath), "%s/%s", badRoot, GOLF_ROUNDS_DIR);
  FILE* obstruction = fopen(roundsPath, "wb");
  assert(obstruction != nullptr && fclose(obstruction) == 0);
  const RoundArchiveResult failed = archiveGolfRound(round, filename, sizeof(filename));
  assert(failed == RoundArchiveResult::Failed);
  assert(finishDestination(failed == RoundArchiveResult::Complete) == FinishDestination::ArchiveError);
  assert(RoundStore::read(loaded).status == GolfJsonStatus::Ok);
}

}  // namespace

int main() {
  testScorecardAndStats();
  testReview();
  testSummary();
  testFinishWalk();
  return 0;
}
