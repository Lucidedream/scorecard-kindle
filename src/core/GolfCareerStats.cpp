#include "GolfCareerStats.h"


#include <stdio.h>

#include "GolfStats.h"

namespace {

uint8_t holesInRound(const GolfRound& round) {
  return round.holeCount < GolfRound::MAX_HOLES ? round.holeCount : GolfRound::MAX_HOLES;
}

bool holeEntered(const GolfPlayerScore& score, const uint8_t hole) {
  return static_cast<uint16_t>(score.in100[hole]) + score.out100[hole] != 0;
}

bool parKnown(const uint8_t par) { return par >= 3 && par <= 6; }

// Gross for the nine starting at `first`; 0 when any of its holes is not entered
// (a partial nine is not a candidate record).
uint16_t nineGross(const GolfRound& round, const GolfPlayerScore& score, const uint8_t first) {
  uint16_t total = 0;
  for (uint8_t hole = first; hole < first + 9; ++hole) {
    if (!holeEntered(score, hole)) return 0;
    total = static_cast<uint16_t>(total + golfHoleScore(round, score, hole));
  }
  return total;
}

}  // namespace

void golfFoldCareerRound(const GolfRound& round, const uint8_t slot, GolfCareerTally& tally) {
  if (round.holeCount != 18 || slot >= GolfRound::MAX_PLAYERS) return;
  const GolfPlayer& player = round.players[slot];
  if (!golfPlayerIsEnabled(player)) return;
  const GolfPlayerScore& score = player.score;
  const uint8_t holes = holesInRound(round);

  ++tally.rounds;
  const uint16_t gross = golfScore(round, score);
  tally.strokes += gross;
  tally.parTotal += golfParTotal(round, score);

  if (golfHasPar(round)) {
    ++tally.parRounds;
    tally.toParTotal += golfToPar(round, score);
  }

  // Score distribution and most-pars, over entered par-known holes.
  uint8_t roundPars = 0;
  for (uint8_t hole = 0; hole < holes; ++hole) {
    if (!holeEntered(score, hole) || !parKnown(round.par[hole])) continue;
    const int16_t d = golfScoreVsPar(round, score, hole);
    uint8_t bucket;
    if (d <= -2) {
      bucket = 0;
    } else if (d == -1) {
      bucket = 1;
    } else if (d == 0) {
      bucket = 2;
    } else if (d == 1) {
      bucket = 3;
    } else if (d == 2) {
      bucket = 4;
    } else {
      bucket = 5;
    }
    ++tally.dist[bucket];
    if (d == 0) ++roundPars;
  }
  if (roundPars > tally.mostPars) tally.mostPars = roundPars;

  // Average by par type (par 3 / 4 / 5; par 6 has no bucket).
  for (uint8_t hole = 0; hole < holes; ++hole) {
    if (!holeEntered(score, hole)) continue;
    const uint8_t par = round.par[hole];
    if (par < 3 || par > 5) continue;
    const uint8_t t = static_cast<uint8_t>(par - 3);
    tally.parStrokes[t] = static_cast<uint16_t>(tally.parStrokes[t] + golfHoleScore(round, score, hole));
    ++tally.parHoles[t];
  }

  // Around the green.
  tally.scrambles = static_cast<uint16_t>(tally.scrambles + golfScrambles(round, score));
  tally.scrambleChances = static_cast<uint16_t>(tally.scrambleChances + golfScrambleChances(round, score));
  tally.sandSaves = static_cast<uint16_t>(tally.sandSaves + golfSandSaves(round, score));
  tally.sandSaveChances = static_cast<uint16_t>(tally.sandSaveChances + golfSandSaveChances(round, score));
  for (uint8_t hole = 0; hole < holes; ++hole) {
    if (!holeEntered(score, hole)) continue;
    ++tally.holesPlayed;
    if (score.putts[hole] >= 3) ++tally.threePuttHoles;
    if (golfGreenInRegulation(round, score, hole)) {
      tally.puttsOnGir = static_cast<uint16_t>(tally.puttsOnGir + score.putts[hole]);
      ++tally.girHoles;
    }
  }

  // Longest bogey-free run: consecutive entered holes at par or better. An
  // entered hole with unknown par scores as "even" and extends the run; a
  // not-entered hole breaks it.
  uint8_t run = 0;
  for (uint8_t hole = 0; hole < holes; ++hole) {
    if (holeEntered(score, hole) && golfScoreVsPar(round, score, hole) <= 0) {
      ++run;
      if (run > tally.longestBogeyFreeRun) tally.longestBogeyFreeRun = run;
    } else {
      run = 0;
    }
  }

  // Records that need a complete 18.
  if (golfThru(round, score) == 18) {
    if (tally.lowestRound == 0 || gross < tally.lowestRound) {
      tally.lowestRound = gross;
      snprintf(tally.lowestCourse, sizeof(tally.lowestCourse), "%s", round.courseName);
    }
    const uint16_t putts = golfPuttsTotal(round, score);
    if (tally.fewestPutts == 0 || putts < tally.fewestPutts) tally.fewestPutts = putts;
  }

  const uint16_t front = nineGross(round, score, 0);
  const uint16_t back = nineGross(round, score, 9);
  if (front != 0 && (tally.lowestNine == 0 || front < tally.lowestNine)) tally.lowestNine = front;
  if (back != 0 && (tally.lowestNine == 0 || back < tally.lowestNine)) tally.lowestNine = back;
}

