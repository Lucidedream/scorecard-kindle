#pragma once

#include <stdint.h>
#include <string.h>


inline constexpr uint8_t GOLF_MAX_HOLES = 18;
inline constexpr uint8_t GOLF_MAX_PLAYERS = 4;
inline constexpr uint8_t GOLF_MAX_PENALTIES_PER_HOLE = 8;
inline constexpr uint8_t GOLF_TEE_CAPACITY = 12;

struct GolfPlayerScore {
  uint8_t putts[GOLF_MAX_HOLES];
  uint8_t in100[GOLF_MAX_HOLES];
  uint8_t out100[GOLF_MAX_HOLES];
  uint8_t penaltyCount[GOLF_MAX_HOLES];
  uint8_t penaltyEvents[GOLF_MAX_HOLES][GOLF_MAX_PENALTIES_PER_HOLE / 2];
  uint8_t fairwayHit[3];
  uint8_t greensideBunker[3];
};

struct GolfPlayer {
  static constexpr uint8_t NAME_CAPACITY = 24;
  char name[NAME_CAPACITY];
  char tee[GOLF_TEE_CAPACITY];
  uint16_t yards[GOLF_MAX_HOLES];
  GolfPlayerScore score;
};

inline void golfSetTeeString(char* dest, const char* tee) {
  memset(dest, 0, GOLF_TEE_CAPACITY);
  if (tee == nullptr) return;
  const size_t length = strlen(tee);
  memcpy(dest, tee, length < GOLF_TEE_CAPACITY ? length : GOLF_TEE_CAPACITY - 1);
}

inline void golfSetTee(GolfPlayer& player, const char* tee) { golfSetTeeString(player.tee, tee); }

struct GolfRound {
  static constexpr uint8_t MAX_HOLES = GOLF_MAX_HOLES;
  static constexpr uint8_t MAX_PLAYERS = GOLF_MAX_PLAYERS;
  static constexpr uint8_t MAX_PENALTIES_PER_HOLE = GOLF_MAX_PENALTIES_PER_HOLE;
  static constexpr uint8_t NO_PLAYER = UINT8_MAX;

  char courseName[40];
  uint16_t dateYmd;
  uint8_t holeCount;
  uint8_t currentHole;
  uint8_t currentPlayer;
  uint8_t par[MAX_HOLES];
  uint8_t si[MAX_HOLES];
  bool hasSi;
  GolfPlayer players[MAX_PLAYERS];
};

inline constexpr char GOLF_DEFAULT_PLAYER_NAMES[GolfRound::MAX_PLAYERS][GolfPlayer::NAME_CAPACITY] = {
    "Noah", "Player 2", "Player 3", "Player 4"};

inline void initializeGolfPlayerDefaults(GolfRound& round) {
  for (uint8_t player = 0; player < GolfRound::MAX_PLAYERS; ++player) {
    for (uint8_t byte = 0; byte < GolfPlayer::NAME_CAPACITY; ++byte) {
      round.players[player].name[byte] = GOLF_DEFAULT_PLAYER_NAMES[player][byte];
    }
    round.players[player].tee[0] = '\0';
  }
}

constexpr bool golfPlayerIsEnabled(const GolfPlayer& player) { return player.tee[0] != '\0'; }

constexpr uint8_t golfEnabledPlayerCount(const GolfRound& round) {
  uint8_t count = 0;
  for (uint8_t slot = 0; slot < GolfRound::MAX_PLAYERS; ++slot) {
    if (golfPlayerIsEnabled(round.players[slot])) ++count;
  }
  return count;
}

inline bool golfDisablePlayer(GolfRound& round, const uint8_t slot) {
  if (slot >= GolfRound::MAX_PLAYERS || !golfPlayerIsEnabled(round.players[slot])) return false;
  golfSetTee(round.players[slot], "");
  round.players[slot].score = GolfPlayerScore{};
  memset(round.players[slot].yards, 0, sizeof(round.players[slot].yards));
  return true;
}

static_assert(sizeof(GolfPlayerScore) == 150);
static_assert(sizeof(GolfPlayer) == 222);
static_assert(sizeof(GolfRound) == 970);
static_assert(__is_standard_layout(GolfPlayerScore));
static_assert(__is_trivially_copyable(GolfPlayerScore));
static_assert(__is_standard_layout(GolfPlayer));
static_assert(__is_trivially_copyable(GolfPlayer));
static_assert(__is_standard_layout(GolfRound));
static_assert(__is_trivially_copyable(GolfRound));
