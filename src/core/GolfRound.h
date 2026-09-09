#pragma once

#include <cstdint>
#include <type_traits>

inline constexpr uint8_t GOLF_MAX_HOLES = 18;
inline constexpr uint8_t GOLF_MAX_PLAYERS = 4;
inline constexpr uint8_t GOLF_MAX_PENALTIES_PER_HOLE = 8;

enum class TeeSelection : uint8_t { NotPlay = 0, Blue = 1, White = 2 };

struct GolfPlayerScore {
  uint8_t putts[GOLF_MAX_HOLES];
  uint8_t in100[GOLF_MAX_HOLES];
  uint8_t out100[GOLF_MAX_HOLES];
  uint8_t penaltyCount[GOLF_MAX_HOLES];
  uint8_t penaltyEvents[GOLF_MAX_HOLES][GOLF_MAX_PENALTIES_PER_HOLE / 2];
  // Bit `hole` (fairwayHit[hole / 8] & (1 << (hole % 8))) set means the tee shot
  // found the fairway on that hole. Not a stroke and not a penalty.
  uint8_t fairwayHit[3];
};

struct GolfPlayer {
  static constexpr uint8_t NAME_CAPACITY = 24;

  char name[NAME_CAPACITY];
  TeeSelection tee;
  uint16_t yards[GOLF_MAX_HOLES];
  GolfPlayerScore score;
};

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
  }
}

constexpr bool golfPlayerIsEnabled(const GolfPlayer& player) { return player.tee != TeeSelection::NotPlay; }

static_assert(sizeof(TeeSelection) == 1);
static_assert(sizeof(GolfPlayerScore) == 147);
static_assert(sizeof(GolfPlayer) == 210);
static_assert(sizeof(GolfRound) == 922);
static_assert(std::is_standard_layout_v<GolfPlayerScore>);
static_assert(std::is_trivially_copyable_v<GolfPlayerScore>);
static_assert(std::is_standard_layout_v<GolfPlayer>);
static_assert(std::is_trivially_copyable_v<GolfPlayer>);
static_assert(std::is_standard_layout_v<GolfRound>);
static_assert(std::is_trivially_copyable_v<GolfRound>);
