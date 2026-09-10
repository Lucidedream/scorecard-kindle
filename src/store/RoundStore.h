#pragma once

#include "GolfJson.h"

class RoundStore {
 public:
  static bool write(const GolfRound& round);
  static GolfJsonResult read(GolfRound& round);
  static bool writeArchiveMarker(const char* filename);
  static bool clear();
};
