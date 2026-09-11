#pragma once

#include <stddef.h>
#include <stdint.h>

#include "SummaryScreen.h"
#include "core/GolfRound.h"
#include "store/GolfPaths.h"

inline constexpr size_t GOLF_HISTORY_LIMIT = 50;

struct HistoryRow {
  char date[16];
  char course[64];
  uint8_t holes;
  uint8_t playerSlot;
  char playerName[24];
  uint16_t strokes;
  uint16_t par;
  uint16_t putts;
  uint16_t in100;
  uint16_t out100;
  uint16_t hazards;
  uint16_t obs;
  uint16_t fairways;
  uint16_t fairwayHoles;
  uint16_t gir;
  uint16_t girHoles;
  char file[GOLF_ARCHIVE_NAME_CAPACITY];
};

struct HistoryReadResult {
  bool readable;
  bool olderRoundsExist;
  size_t count;
};

struct HistoryPlayer {
  uint8_t slot;
  char name[24];
  size_t roundCount;
};

HistoryReadResult golfReadHistoryIndex(HistoryRow* rows, size_t capacity);
size_t golfHistoryPlayers(const HistoryRow* rows, size_t count, HistoryPlayer* players, size_t capacity);
size_t golfHistoryRowsForPlayer(const HistoryRow* rows, size_t count, uint8_t slot,
                                const HistoryRow** matches, size_t capacity);
size_t golfHistoryFileRowCount(const HistoryRow* rows, size_t count, const char* file);
bool golfReadHistoryRound(const char* file, GolfRound& round);
SummaryView golfHistorySummaryView(const HistoryRow& row);

