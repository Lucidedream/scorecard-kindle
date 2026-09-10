#include "GolfStats.h"


#include "GolfPenalty.h"

namespace {

uint8_t holesInRound(const GolfRound& round) {
  return round.holeCount < GolfRound::MAX_HOLES ? round.holeCount : GolfRound::MAX_HOLES;
}

bool isEntered(const GolfPlayerScore& score, const uint8_t hole) {
  return static_cast<uint16_t>(score.in100[hole]) + score.out100[hole] != 0;
}

bool parIsGirEligible(const uint8_t par) { return par >= 3 && par <= 5; }

bool parIsKnown(const uint8_t par) { return par >= 3 && par <= 6; }

}  // namespace

uint8_t golfLongGame(const GolfRound& round, const GolfPlayerScore& score, const uint8_t hole) {
  if (hole >= holesInRound(round) || !isEntered(score, hole)) {
    return 0;
  }
  return score.out100[hole];
}

uint16_t golfPenaltyTotal(const GolfRound& round, const GolfPlayerScore& score) {
  return golfPenaltyStrokesForRound(score, holesInRound(round));
}

uint16_t golfHoleScore(const GolfRound& round, const GolfPlayerScore& score, const uint8_t hole) {
  if (hole >= holesInRound(round)) return 0;
  return static_cast<uint16_t>(score.in100[hole]) + score.out100[hole] + golfPenaltyStrokesForHole(score, hole);
}

uint16_t golfScore(const GolfRound& round, const GolfPlayerScore& score) {
  uint16_t total = 0;
  for (uint8_t hole = 0; hole < holesInRound(round); ++hole) {
    if (isEntered(score, hole)) {
      total += golfHoleScore(round, score, hole);
    }
  }
  return total;
}

uint16_t golfParTotal(const GolfRound& round, const GolfPlayerScore& score) {
  uint16_t total = 0;
  for (uint8_t hole = 0; hole < holesInRound(round); ++hole) {
    if (isEntered(score, hole)) total += round.par[hole];
  }
  return total;
}

bool golfHasPar(const GolfRound& round) {
  if (holesInRound(round) == 0) return false;
  for (uint8_t hole = 0; hole < holesInRound(round); ++hole) {
    if (round.par[hole] < 3 || round.par[hole] > 6) return false;
  }
  return true;
}

int16_t golfToPar(const GolfRound& round, const GolfPlayerScore& score) {
  int16_t total = 0;
  for (uint8_t hole = 0; hole < holesInRound(round); ++hole) {
    if (isEntered(score, hole) && round.par[hole] != 0) {
      total += static_cast<int16_t>(golfHoleScore(round, score, hole)) - round.par[hole];
    }
  }
  return total;
}

uint8_t golfThru(const GolfRound& round, const GolfPlayerScore& score) {
  uint8_t total = 0;
  for (uint8_t hole = 0; hole < holesInRound(round); ++hole) {
    if (isEntered(score, hole)) {
      ++total;
    }
  }
  return total;
}

uint16_t golfPuttsTotal(const GolfRound& round, const GolfPlayerScore& score) {
  uint16_t total = 0;
  for (uint8_t hole = 0; hole < holesInRound(round); ++hole) {
    if (isEntered(score, hole)) {
      total += score.putts[hole];
    }
  }
  return total;
}

uint16_t golfIn100Total(const GolfRound& round, const GolfPlayerScore& score) {
  uint16_t total = 0;
  for (uint8_t hole = 0; hole < holesInRound(round); ++hole) {
    if (isEntered(score, hole)) {
      total += score.in100[hole];
    }
  }
  return total;
}

uint16_t golfShortTotal(const GolfRound& round, const GolfPlayerScore& score) {
  return static_cast<uint16_t>(golfIn100Total(round, score) - golfPuttsTotal(round, score));
}

uint16_t golfLongTotal(const GolfRound& round, const GolfPlayerScore& score) {
  uint16_t total = 0;
  for (uint8_t hole = 0; hole < holesInRound(round); ++hole) {
    if (isEntered(score, hole)) {
      total += golfLongGame(round, score, hole);
    }
  }
  return total;
}

uint8_t golfOnePutts(const GolfRound& round, const GolfPlayerScore& score) {
  uint8_t total = 0;
  for (uint8_t hole = 0; hole < holesInRound(round); ++hole) {
    if (isEntered(score, hole) && score.putts[hole] == 1) {
      ++total;
    }
  }
  return total;
}

uint8_t golfThreePutts(const GolfRound& round, const GolfPlayerScore& score) {
  uint8_t total = 0;
  for (uint8_t hole = 0; hole < holesInRound(round); ++hole) {
    if (isEntered(score, hole) && score.putts[hole] >= 3) {
      ++total;
    }
  }
  return total;
}

uint8_t golfWorstHoles(const GolfRound& round, const GolfPlayerScore& score, GolfWorstHole* holes,
                       const uint8_t capacity) {
  if (holes == nullptr || capacity == 0) {
    return 0;
  }

  uint8_t count = 0;
  for (uint8_t hole = 0; hole < holesInRound(round); ++hole) {
    if (!isEntered(score, hole) || round.par[hole] == 0) {
      continue;
    }

    GolfWorstHole candidate{
        hole, static_cast<int16_t>(static_cast<int16_t>(golfHoleScore(round, score, hole)) - round.par[hole])};
    uint8_t position = count;
    if (count < capacity) {
      ++count;
    } else {
      if (holes[capacity - 1].toPar >= candidate.toPar) {
        continue;
      }
      position = capacity - 1;
    }
    while (position > 0 && holes[position - 1].toPar < candidate.toPar) {
      holes[position] = holes[position - 1];
      --position;
    }
    holes[position] = candidate;
  }
  return count;
}

bool golfGreenInRegulation(const GolfRound& round, const GolfPlayerScore& score, const uint8_t hole) {
  if (hole >= holesInRound(round) || !isEntered(score, hole) || !parIsGirEligible(round.par[hole])) {
    return false;
  }
  const int strokesToGreen = static_cast<int>(golfHoleScore(round, score, hole)) - score.putts[hole];
  return strokesToGreen <= round.par[hole] - 2;
}

uint8_t golfGreensInRegulation(const GolfRound& round, const GolfPlayerScore& score) {
  uint8_t total = 0;
  for (uint8_t hole = 0; hole < holesInRound(round); ++hole) {
    if (golfGreenInRegulation(round, score, hole)) ++total;
  }
  return total;
}

uint8_t golfGreensEligible(const GolfRound& round, const GolfPlayerScore& score) {
  uint8_t total = 0;
  for (uint8_t hole = 0; hole < holesInRound(round); ++hole) {
    if (isEntered(score, hole) && parIsGirEligible(round.par[hole])) ++total;
  }
  return total;
}

uint8_t golfFairwaysHit(const GolfRound& round, const GolfPlayerScore& score) {
  uint8_t total = 0;
  for (uint8_t hole = 0; hole < holesInRound(round); ++hole) {
    if (isEntered(score, hole) && golfFairwayHit(score, hole)) ++total;
  }
  return total;
}

uint8_t golfFairwaysEligible(const GolfRound& round, const GolfPlayerScore& score) {
  if (!golfHasPar(round)) return 0;
  uint8_t total = 0;
  for (uint8_t hole = 0; hole < holesInRound(round); ++hole) {
    if (isEntered(score, hole) && (round.par[hole] == 4 || round.par[hole] == 5)) ++total;
  }
  return total;
}

namespace {

bool madeParOrBetter(const GolfRound& round, const GolfPlayerScore& score, const uint8_t hole) {
  return parIsKnown(round.par[hole]) && golfHoleScore(round, score, hole) <= round.par[hole];
}

}  // namespace

int16_t golfScoreVsPar(const GolfRound& round, const GolfPlayerScore& score, const uint8_t hole) {
  if (hole >= holesInRound(round) || !isEntered(score, hole) || !parIsKnown(round.par[hole])) return 0;
  return static_cast<int16_t>(static_cast<int16_t>(golfHoleScore(round, score, hole)) - round.par[hole]);
}

uint8_t golfScrambleChances(const GolfRound& round, const GolfPlayerScore& score) {
  uint8_t total = 0;
  for (uint8_t hole = 0; hole < holesInRound(round); ++hole) {
    if (isEntered(score, hole) && parIsGirEligible(round.par[hole]) && !golfGreenInRegulation(round, score, hole)) {
      ++total;
    }
  }
  return total;
}

uint8_t golfScrambles(const GolfRound& round, const GolfPlayerScore& score) {
  uint8_t total = 0;
  for (uint8_t hole = 0; hole < holesInRound(round); ++hole) {
    if (isEntered(score, hole) && parIsGirEligible(round.par[hole]) && !golfGreenInRegulation(round, score, hole) &&
        madeParOrBetter(round, score, hole)) {
      ++total;
    }
  }
  return total;
}

uint8_t golfSandSaveChances(const GolfRound& round, const GolfPlayerScore& score) {
  uint8_t total = 0;
  for (uint8_t hole = 0; hole < holesInRound(round); ++hole) {
    if (isEntered(score, hole) && golfGreensideBunker(score, hole)) ++total;
  }
  return total;
}

uint8_t golfSandSaves(const GolfRound& round, const GolfPlayerScore& score) {
  uint8_t total = 0;
  for (uint8_t hole = 0; hole < holesInRound(round); ++hole) {
    if (isEntered(score, hole) && golfGreensideBunker(score, hole) && madeParOrBetter(round, score, hole)) ++total;
  }
  return total;
}

