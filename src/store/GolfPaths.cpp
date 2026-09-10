#include "GolfPaths.h"

#include <stdio.h>
#include <stdlib.h>

const char* golfStorageRoot() {
  const char* override = getenv("SCORECARD_DIR");
  return override != nullptr && override[0] != '\0' ? override : GOLF_DEFAULT_ROOT;
}

bool golfPath(char* output, const size_t capacity, const char* relative) {
  if (output == nullptr || capacity == 0 || relative == nullptr) return false;
  const int length = snprintf(output, capacity, "%s/%s", golfStorageRoot(), relative);
  return length >= 0 && static_cast<size_t>(length) < capacity;
}
