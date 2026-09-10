#pragma once

#include <stdint.h>

#include "core/GolfRound.h"

struct SummaryView {
  bool hasPar;
  uint8_t playerSlot;
  uint8_t playerCount;
  char header[64];
  char player[GolfPlayer::NAME_CAPACITY + 8];
  char score[16];
  char toPar[16];
  char putts[16];
  char in100[16];
  char longGame[16];
  char penalties[16];
  char fairways[16];
  char greens[16];
};

enum class FinishDestination : uint8_t { Summary, ArchiveError };

SummaryView summaryView(const GolfRound& round, uint8_t playerSlot);
FinishDestination finishDestination(bool archiveComplete);

