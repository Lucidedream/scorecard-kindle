#include "Battery.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace {

bool readIntFile(const char* path, int& value) {
  FILE* file = fopen(path, "r");
  if (file == nullptr) return false;
  char buffer[16] = {};
  const bool read = fgets(buffer, sizeof(buffer), file) != nullptr;
  fclose(file);
  if (!read) return false;
  char* end = nullptr;
  const long parsed = strtol(buffer, &end, 10);
  if (end == buffer || parsed < 0 || parsed > 100) return false;
  value = static_cast<int>(parsed);
  return true;
}

// A power-supply node without a "type" file is not excluded -- some kernels
// only expose one for AC/USB nodes and leave it off the battery itself.
bool looksLikeBattery(const char* dir) {
  char path[256];
  if (snprintf(path, sizeof(path), "%s/type", dir) >= static_cast<int>(sizeof(path))) return true;
  FILE* file = fopen(path, "r");
  if (file == nullptr) return true;
  char buffer[32] = {};
  const bool read = fgets(buffer, sizeof(buffer), file) != nullptr;
  fclose(file);
  return !read || strncmp(buffer, "Battery", 7) == 0;
}

}  // namespace

int readBatteryPercent() {
  const char* base = getenv("BATTERY_SUPPLY_DIR");
  if (base == nullptr || base[0] == '\0') base = "/sys/class/power_supply";
  DIR* directory = opendir(base);
  if (directory == nullptr) return -1;
  int result = -1;
  while (const dirent* entry = readdir(directory)) {
    if (entry->d_name[0] == '.') continue;
    char dir[256];
    if (snprintf(dir, sizeof(dir), "%s/%s", base, entry->d_name) >= static_cast<int>(sizeof(dir))) continue;
    if (!looksLikeBattery(dir)) continue;
    char capacityPath[288];
    if (snprintf(capacityPath, sizeof(capacityPath), "%s/capacity", dir) >=
        static_cast<int>(sizeof(capacityPath))) {
      continue;
    }
    if (readIntFile(capacityPath, result)) break;
  }
  closedir(directory);
  return result;
}
