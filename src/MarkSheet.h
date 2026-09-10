#pragma once

#include <stdint.h>

#include "HitTester.h"
#include "core/GolfPenalty.h"

struct MarkSheetState {
  GolfField field;
  bool holeFull;
};

struct MarkSheetLayout {
  Rect scrim;
  Rect sheet;
  Rect title;
  Rect fieldLabel;
  Rect out100Segment;
  Rect in100Segment;
  Rect bunkerRow;
  Rect hazardRow;
  Rect hazardMinus;
  Rect hazardPlus;
  Rect obRow;
  Rect obMinus;
  Rect obPlus;
  Rect status;
  Rect done;
};

struct MarkSheetView {
  MarkSheetLayout layout;
  GolfField field;
  bool bunker;
  bool holeFull;
  bool hazardMinusEnabled;
  bool obMinusEnabled;
  uint8_t hazards;
  uint8_t obs;
  char title[32];
};

enum class MarkMutationResult : uint8_t { Changed, NoChange, HoleFull };

MarkSheetState initialMarkSheetState();
MarkSheetLayout markSheetLayout();
MarkSheetView markSheetView(const GolfRound& round, const MarkSheetState& state);
uint8_t golfPenaltyMarkersForField(const GolfPlayerScore& score, uint8_t hole, GolfField field);
uint8_t golfPenaltyMarkersForField(const GolfPlayerScore& score, uint8_t hole, GolfField field,
                                   GolfPenaltyKind kind);
bool golfLatestPenaltyForFieldIs(const GolfPlayerScore& score, uint8_t hole, GolfField field,
                                 GolfPenaltyKind kind);
bool toggleGolfFairway(GolfRound& round);
bool toggleMarkBunker(GolfRound& round);
void selectMarkField(MarkSheetState& state, GolfField field);
MarkMutationResult changeMarkPenalty(GolfRound& round, MarkSheetState& state, GolfPenaltyKind kind,
                                     bool increment);
