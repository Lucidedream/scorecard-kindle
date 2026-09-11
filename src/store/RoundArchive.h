#pragma once

#include <stddef.h>

#include "core/GolfRound.h"

enum class RoundArchiveResult : uint8_t { Failed, Complete };

void golfSlug(const char* course, char* output, size_t capacity);
RoundArchiveResult archiveGolfRound(const GolfRound& round, char* filename = nullptr,
                                    size_t filenameCapacity = 0);
bool removeRound(const char* file);
bool removePlayerFromRound(const char* file, uint8_t playerSlot);
