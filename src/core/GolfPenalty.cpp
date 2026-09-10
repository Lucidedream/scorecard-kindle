#include "GolfPenalty.h"


namespace {

constexpr uint8_t FIELD_MASK = 0x03;
constexpr uint8_t KIND_MASK = 0x04;
constexpr uint8_t RESERVED_MASK = 0x08;

bool validHole(const uint8_t hole) { return hole < GolfRound::MAX_HOLES; }

uint8_t packedAt(const GolfPlayerScore& score, const uint8_t hole, const uint8_t index) {
  const uint8_t packed = score.penaltyEvents[hole][index / 2];
  return index % 2 == 0 ? static_cast<uint8_t>(packed & 0x0f) : static_cast<uint8_t>(packed >> 4);
}

void storeAt(GolfPlayerScore& score, const uint8_t hole, const uint8_t index, const uint8_t packed) {
  uint8_t& target = score.penaltyEvents[hole][index / 2];
  const uint8_t shift = index % 2 == 0 ? 0 : 4;
  target = static_cast<uint8_t>((target & ~(0x0f << shift)) | ((packed & 0x0f) << shift));
}

uint8_t holesInRound(const uint8_t holeCount) {
  return holeCount < GolfRound::MAX_HOLES ? holeCount : GolfRound::MAX_HOLES;
}

}  // namespace

uint8_t golfPackPenaltyEvent(const GolfField field, const GolfPenaltyKind kind) {
  return static_cast<uint8_t>(static_cast<uint8_t>(field) | (static_cast<uint8_t>(kind) << 2));
}

bool golfUnpackPenaltyEvent(const uint8_t packed, GolfPenaltyEvent& event) {
  if ((packed & RESERVED_MASK) != 0 || (packed & FIELD_MASK) > static_cast<uint8_t>(GolfField::Out100)) return false;
  event.field = static_cast<GolfField>(packed & FIELD_MASK);
  event.kind = static_cast<GolfPenaltyKind>((packed & KIND_MASK) >> 2);
  return true;
}

bool golfPenaltyEventAt(const GolfPlayerScore& score, const uint8_t hole, const uint8_t index,
                        GolfPenaltyEvent& event) {
  return validHole(hole) && index < score.penaltyCount[hole] && index < GolfRound::MAX_PENALTIES_PER_HOLE &&
         golfUnpackPenaltyEvent(packedAt(score, hole, index), event);
}

GolfPenaltyMutationStatus golfAppendPenalty(GolfPlayerScore& score, const uint8_t hole, const GolfField field,
                                            const GolfPenaltyKind kind) {
  if (!validHole(hole)) return GolfPenaltyMutationStatus::InvalidHole;
  // Putts takes no new penalty: you cannot hit a hazard or go OB with a putt
  // (CONTRACTS-V2 §31.2). Pre-existing Putts markers still read and remove.
  if (field == GolfField::Putts || static_cast<uint8_t>(field) > static_cast<uint8_t>(GolfField::Out100) ||
      static_cast<uint8_t>(kind) > static_cast<uint8_t>(GolfPenaltyKind::Ob)) {
    return GolfPenaltyMutationStatus::InvalidEvent;
  }
  if (score.penaltyCount[hole] >= GolfRound::MAX_PENALTIES_PER_HOLE) {
    return GolfPenaltyMutationStatus::HoleFull;
  }

  const GolfMutationResult mutation = incrementGolfCounter(score, hole, field);
  if (!mutation.changed) return GolfPenaltyMutationStatus::CounterClamped;
  if (field == GolfField::Putts && !mutation.carriedIn100) {
    if (!incrementGolfCounter(score, hole, GolfField::In100).changed) {
      decrementGolfCounter(score, hole, GolfField::Putts);
      return GolfPenaltyMutationStatus::CounterClamped;
    }
  }
  storeAt(score, hole, score.penaltyCount[hole], golfPackPenaltyEvent(field, kind));
  ++score.penaltyCount[hole];
  return GolfPenaltyMutationStatus::Changed;
}

GolfPenaltyMutationStatus golfRemoveLatestPenalty(GolfPlayerScore& score, const uint8_t hole, const GolfField field) {
  if (!validHole(hole)) return GolfPenaltyMutationStatus::InvalidHole;
  if (static_cast<uint8_t>(field) > static_cast<uint8_t>(GolfField::Out100)) {
    return GolfPenaltyMutationStatus::InvalidEvent;
  }

  uint8_t removeIndex = score.penaltyCount[hole];
  while (removeIndex > 0) {
    GolfPenaltyEvent event{};
    --removeIndex;
    if (golfUnpackPenaltyEvent(packedAt(score, hole, removeIndex), event) && event.field == field) break;
  }
  GolfPenaltyEvent removed{};
  if (removeIndex >= score.penaltyCount[hole] || !golfUnpackPenaltyEvent(packedAt(score, hole, removeIndex), removed) ||
      removed.field != field) {
    return GolfPenaltyMutationStatus::NoMarker;
  }

  GolfMutationResult mutation{};
  if (field == GolfField::Putts && score.putts[hole] == score.in100[hole]) {
    mutation = decrementGolfCounter(score, hole, GolfField::In100);
  } else if (field == GolfField::Putts) {
    mutation = decrementGolfCounter(score, hole, GolfField::Putts);
    if (mutation.changed && !decrementGolfCounter(score, hole, GolfField::In100).changed) {
      incrementGolfCounter(score, hole, GolfField::Putts);
      return GolfPenaltyMutationStatus::CounterClamped;
    }
  } else {
    mutation = decrementGolfCounter(score, hole, field);
  }
  if (!mutation.changed) return GolfPenaltyMutationStatus::CounterClamped;

  for (uint8_t index = removeIndex; index + 1 < score.penaltyCount[hole]; ++index) {
    storeAt(score, hole, index, packedAt(score, hole, static_cast<uint8_t>(index + 1)));
  }
  --score.penaltyCount[hole];
  storeAt(score, hole, score.penaltyCount[hole], 0);
  return GolfPenaltyMutationStatus::Changed;
}

uint8_t golfHazardsForHole(const GolfPlayerScore& score, const uint8_t hole) {
  if (!validHole(hole)) return 0;
  uint8_t hazards = 0;
  const uint8_t count = score.penaltyCount[hole] < GolfRound::MAX_PENALTIES_PER_HOLE
                            ? score.penaltyCount[hole]
                            : GolfRound::MAX_PENALTIES_PER_HOLE;
  for (uint8_t index = 0; index < count; ++index) {
    GolfPenaltyEvent event{};
    if (golfUnpackPenaltyEvent(packedAt(score, hole, index), event) && event.kind == GolfPenaltyKind::Hazard) {
      ++hazards;
    }
  }
  return hazards;
}

uint8_t golfObsForHole(const GolfPlayerScore& score, const uint8_t hole) {
  if (!validHole(hole)) return 0;
  uint8_t obs = 0;
  const uint8_t count = score.penaltyCount[hole] < GolfRound::MAX_PENALTIES_PER_HOLE
                            ? score.penaltyCount[hole]
                            : GolfRound::MAX_PENALTIES_PER_HOLE;
  for (uint8_t index = 0; index < count; ++index) {
    GolfPenaltyEvent event{};
    if (golfUnpackPenaltyEvent(packedAt(score, hole, index), event) && event.kind == GolfPenaltyKind::Ob) ++obs;
  }
  return obs;
}

uint16_t golfHazardsForRound(const GolfPlayerScore& score, const uint8_t holeCount) {
  uint16_t total = 0;
  for (uint8_t hole = 0; hole < holesInRound(holeCount); ++hole) total += golfHazardsForHole(score, hole);
  return total;
}

uint16_t golfObsForRound(const GolfPlayerScore& score, const uint8_t holeCount) {
  uint16_t total = 0;
  for (uint8_t hole = 0; hole < holesInRound(holeCount); ++hole) total += golfObsForHole(score, hole);
  return total;
}

uint16_t golfPenaltyStrokesForHole(const GolfPlayerScore& score, const uint8_t hole) {
  return static_cast<uint16_t>(golfHazardsForHole(score, hole)) +
         static_cast<uint16_t>(golfObsForHole(score, hole)) * 2;
}

uint16_t golfPenaltyStrokesForRound(const GolfPlayerScore& score, const uint8_t holeCount) {
  return static_cast<uint16_t>(golfHazardsForRound(score, holeCount) + golfObsForRound(score, holeCount) * 2);
}

bool golfFairwayHit(const GolfPlayerScore& score, const uint8_t hole) {
  if (!validHole(hole)) return false;
  return (score.fairwayHit[hole / 8] & (1 << (hole % 8))) != 0;
}

void golfSetFairwayHit(GolfPlayerScore& score, const uint8_t hole, const bool hit) {
  if (!validHole(hole)) return;
  const uint8_t mask = static_cast<uint8_t>(1 << (hole % 8));
  if (hit) {
    score.fairwayHit[hole / 8] |= mask;
  } else {
    score.fairwayHit[hole / 8] &= static_cast<uint8_t>(~mask);
  }
}

uint16_t golfFairwayHitsForRound(const GolfPlayerScore& score, const uint8_t holeCount) {
  uint16_t total = 0;
  for (uint8_t hole = 0; hole < holesInRound(holeCount); ++hole) {
    if (golfFairwayHit(score, hole)) ++total;
  }
  return total;
}

bool golfGreensideBunker(const GolfPlayerScore& score, const uint8_t hole) {
  if (!validHole(hole)) return false;
  return (score.greensideBunker[hole / 8] & (1 << (hole % 8))) != 0;
}

void golfSetGreensideBunker(GolfPlayerScore& score, const uint8_t hole, const bool bunkered) {
  if (!validHole(hole)) return;
  const uint8_t mask = static_cast<uint8_t>(1 << (hole % 8));
  if (bunkered) {
    score.greensideBunker[hole / 8] |= mask;
  } else {
    score.greensideBunker[hole / 8] &= static_cast<uint8_t>(~mask);
  }
}

