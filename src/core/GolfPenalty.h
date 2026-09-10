#pragma once

#include <stdint.h>

#include "GolfRound.h"
#include "GolfRules.h"

enum class GolfPenaltyKind : uint8_t { Hazard = 0, Ob = 1 };

struct GolfPenaltyEvent {
  GolfField field;
  GolfPenaltyKind kind;
};

enum class GolfPenaltyMutationStatus : uint8_t {
  Changed,
  NoMarker,
  HoleFull,
  CounterClamped,
  InvalidHole,
  InvalidEvent,
};

uint8_t golfPackPenaltyEvent(GolfField field, GolfPenaltyKind kind);
bool golfUnpackPenaltyEvent(uint8_t packed, GolfPenaltyEvent& event);
bool golfPenaltyEventAt(const GolfPlayerScore& score, uint8_t hole, uint8_t index, GolfPenaltyEvent& event);
GolfPenaltyMutationStatus golfAppendPenalty(GolfPlayerScore& score, uint8_t hole, GolfField field,
                                            GolfPenaltyKind kind);
GolfPenaltyMutationStatus golfRemoveLatestPenalty(GolfPlayerScore& score, uint8_t hole, GolfField field);
uint8_t golfHazardsForHole(const GolfPlayerScore& score, uint8_t hole);
uint8_t golfObsForHole(const GolfPlayerScore& score, uint8_t hole);
uint16_t golfHazardsForRound(const GolfPlayerScore& score, uint8_t holeCount);
uint16_t golfObsForRound(const GolfPlayerScore& score, uint8_t holeCount);
uint16_t golfPenaltyStrokesForHole(const GolfPlayerScore& score, uint8_t hole);
uint16_t golfPenaltyStrokesForRound(const GolfPlayerScore& score, uint8_t holeCount);

// Fairway hit is a dedicated per-hole bit, not a GolfPenaltyKind (see CONTRACTS-V2
// §31.5). It records a good outcome and adds no stroke. These are plain bit
// accessors: they do not seed, touch counters, or consider par -- callers seed
// first, exactly as every mutation path must (§13.1).
bool golfFairwayHit(const GolfPlayerScore& score, uint8_t hole);
void golfSetFairwayHit(GolfPlayerScore& score, uint8_t hole, bool hit);
uint16_t golfFairwayHitsForRound(const GolfPlayerScore& score, uint8_t holeCount);

// Greenside-bunker bit (CONTRACTS-V2 §33.1): the same plain per-hole bit
// accessors as the fairway pair -- no seeding, no counters, no par. Set means
// the ball lay in a greenside bunker on the way to the hole.
bool golfGreensideBunker(const GolfPlayerScore& score, uint8_t hole);
void golfSetGreensideBunker(GolfPlayerScore& score, uint8_t hole, bool bunkered);
