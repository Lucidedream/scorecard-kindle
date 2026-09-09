#include "GolfRules.h"

#if defined(CROSSPOINT_GOLF)

namespace {

constexpr uint8_t MAX_COUNTER = 99;

GolfMutationResult clamped() { return {false, false, false}; }

GolfMutationResult changed(bool carriedIn100 = false, bool loweredPutts = false) {
  return {true, carriedIn100, loweredPutts};
}

bool isValidHole(const uint8_t hole) { return hole < GolfRound::MAX_HOLES; }

}  // namespace

bool seedGolfHoleAtPar(GolfPlayerScore& score, const uint8_t hole, const uint8_t par) {
  if (!isValidHole(hole) || par < 3 || score.in100[hole] != 0 || score.out100[hole] != 0) {
    return false;
  }
  score.putts[hole] = 2;
  score.in100[hole] = 2;
  score.out100[hole] = static_cast<uint8_t>(par - 2);
  return true;
}

GolfMutationResult incrementGolfCounter(GolfPlayerScore& score, const uint8_t hole, const GolfField field) {
  if (!isValidHole(hole)) {
    return clamped();
  }

  switch (field) {
    case GolfField::Putts: {
      if (score.putts[hole] >= MAX_COUNTER || score.in100[hole] >= MAX_COUNTER) return clamped();
      ++score.putts[hole];
      const bool carried = score.putts[hole] > score.in100[hole];
      if (carried) score.in100[hole] = score.putts[hole];
      return changed(carried);
    }
    case GolfField::In100:
      if (score.in100[hole] >= MAX_COUNTER) return clamped();
      ++score.in100[hole];
      return changed();
    case GolfField::Out100:
      if (score.out100[hole] >= MAX_COUNTER) return clamped();
      ++score.out100[hole];
      return changed();
  }

  return clamped();
}

GolfMutationResult decrementGolfCounter(GolfPlayerScore& score, const uint8_t hole, const GolfField field) {
  if (!isValidHole(hole)) {
    return clamped();
  }

  switch (field) {
    case GolfField::Putts:
      if (score.putts[hole] == 0) return clamped();
      --score.putts[hole];
      return changed();
    case GolfField::In100: {
      if (score.in100[hole] == 0) return clamped();
      --score.in100[hole];
      const bool lowered = score.putts[hole] > score.in100[hole];
      if (lowered) score.putts[hole] = score.in100[hole];
      return changed(false, lowered);
    }
    case GolfField::Out100:
      if (score.out100[hole] == 0) return clamped();
      --score.out100[hole];
      return changed();
  }

  return clamped();
}

GolfField nextGolfField(const GolfField field) {
  switch (field) {
    case GolfField::Putts:
      return GolfField::In100;
    case GolfField::In100:
      return GolfField::Out100;
    case GolfField::Out100:
      return GolfField::Putts;
  }
  return GolfField::Putts;
}

uint8_t golfFirstEnabledPlayer(const GolfRound& round) {
  for (uint8_t player = 0; player < GolfRound::MAX_PLAYERS; ++player) {
    if (golfPlayerIsEnabled(round.players[player])) return player;
  }
  return GolfRound::NO_PLAYER;
}

uint8_t golfNextEnabledPlayer(const GolfRound& round, const uint8_t player) {
  if (player >= GolfRound::MAX_PLAYERS) return golfFirstEnabledPlayer(round);
  for (uint8_t offset = 1; offset <= GolfRound::MAX_PLAYERS; ++offset) {
    const uint8_t candidate = static_cast<uint8_t>((player + offset) % GolfRound::MAX_PLAYERS);
    if (golfPlayerIsEnabled(round.players[candidate])) return candidate;
  }
  return GolfRound::NO_PLAYER;
}

uint8_t golfPreviousEnabledPlayer(const GolfRound& round, const uint8_t player) {
  if (player >= GolfRound::MAX_PLAYERS) return golfFirstEnabledPlayer(round);
  for (uint8_t offset = 1; offset <= GolfRound::MAX_PLAYERS; ++offset) {
    const uint8_t candidate = static_cast<uint8_t>((player + GolfRound::MAX_PLAYERS - offset) % GolfRound::MAX_PLAYERS);
    if (golfPlayerIsEnabled(round.players[candidate])) return candidate;
  }
  return GolfRound::NO_PLAYER;
}

bool golfIsFinalCommit(const GolfRound& round) {
  if (round.holeCount == 0 || round.currentHole != round.holeCount - 1) return false;
  if (round.currentPlayer >= GolfRound::MAX_PLAYERS) return false;
  const uint8_t next = golfNextEnabledPlayer(round, round.currentPlayer);
  return next != GolfRound::NO_PLAYER && next <= round.currentPlayer;
}

bool advanceGolfTurn(GolfRound& round) {
  if (round.holeCount == 0 || round.holeCount > GolfRound::MAX_HOLES) return false;

  const uint8_t first = golfFirstEnabledPlayer(round);
  if (first == GolfRound::NO_PLAYER) return false;
  if (round.currentHole >= round.holeCount) round.currentHole = 0;
  if (round.currentPlayer >= GolfRound::MAX_PLAYERS || !golfPlayerIsEnabled(round.players[round.currentPlayer])) {
    round.currentPlayer = first;
    return true;
  }

  const uint8_t next = golfNextEnabledPlayer(round, round.currentPlayer);
  if (next == GolfRound::NO_PLAYER) return false;
  const bool wrappedPlayers = next <= round.currentPlayer;
  round.currentPlayer = next;
  if (wrappedPlayers) {
    round.currentHole = static_cast<uint8_t>((round.currentHole + 1) % round.holeCount);
  }
  return true;
}

bool retreatGolfTurn(GolfRound& round) {
  if (round.holeCount == 0 || round.holeCount > GolfRound::MAX_HOLES) return false;

  const uint8_t first = golfFirstEnabledPlayer(round);
  if (first == GolfRound::NO_PLAYER) return false;
  if (round.currentHole >= round.holeCount) round.currentHole = 0;
  if (round.currentPlayer >= GolfRound::MAX_PLAYERS || !golfPlayerIsEnabled(round.players[round.currentPlayer])) {
    round.currentPlayer = first;
    return true;
  }

  const bool wrappedPlayers = round.currentPlayer == first;
  const uint8_t previous = golfPreviousEnabledPlayer(round, round.currentPlayer);
  if (previous == GolfRound::NO_PLAYER) return false;
  round.currentPlayer = previous;
  if (wrappedPlayers) {
    round.currentHole = static_cast<uint8_t>((round.currentHole + round.holeCount - 1) % round.holeCount);
  }
  return true;
}

#endif
