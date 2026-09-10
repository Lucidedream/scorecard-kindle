#pragma once

#include <stddef.h>
#include <stdio.h>

#include "core/GolfRound.h"
#include "core/GolfValidate.h"

enum class GolfJsonStatus : uint8_t {
  Ok,
  Archived,
  IoError,
  InvalidJson,
  RejectedVersion,
  InvalidMetadata,
  ArrayLengthMismatch,
};

struct GolfJsonResult {
  GolfJsonStatus status;
  GolfValidationResult validation;
  char archivedAs[96];
};

bool golfWriteRoundJson(FILE* file, const GolfRound& round, bool stateFile, const char* archivedAs = nullptr);
GolfJsonResult golfReadRoundJson(const char* json, size_t size, bool stateFile, GolfRound& round);
bool golfFormatDate(uint16_t dateYmd, char* output, size_t capacity);
bool golfParseDate(const char* date, uint16_t& dateYmd);
