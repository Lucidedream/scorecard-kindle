#include "RoundStore.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include "GolfPaths.h"

namespace {

bool ensureDirectory(const char* path) {
  if (mkdir(path, 0755) == 0 || errno == EEXIST) return true;
  fprintf(stderr, "Failed to create %s: %s\n", path, strerror(errno));
  return false;
}

bool syncParent(const char* path) {
  char parent[GOLF_PATH_CAPACITY];
  const size_t length = strlen(path);
  if (length >= sizeof(parent)) return false;
  memcpy(parent, path, length + 1);
  char* slash = strrchr(parent, '/');
  if (slash == nullptr) return false;
  *slash = '\0';
  const int descriptor = open(parent, O_RDONLY | O_DIRECTORY);
  if (descriptor < 0) return false;
  const bool ok = fsync(descriptor) == 0;
  close(descriptor);
  return ok;
}

bool atomicWrite(const GolfRound* round, const char* marker) {
  if (!ensureDirectory(golfStorageRoot())) return false;
  char path[GOLF_PATH_CAPACITY];
  char temporary[GOLF_PATH_CAPACITY];
  if (!golfPath(path, sizeof(path), GOLF_STATE_FILE) ||
      snprintf(temporary, sizeof(temporary), "%s.tmp", path) >= static_cast<int>(sizeof(temporary))) return false;
  FILE* file = fopen(temporary, "wb");
  if (file == nullptr) return false;
  static const GolfRound empty{};
  const bool written = golfWriteRoundJson(file, round == nullptr ? empty : *round, true, marker);
  const bool synced = written && fflush(file) == 0 && fsync(fileno(file)) == 0;
  const bool closed = fclose(file) == 0;
  if (!synced || !closed || rename(temporary, path) != 0 || !syncParent(path)) {
    unlink(temporary);
    return false;
  }
  return true;
}

void logRepairs(const GolfValidationResult& validation) {
  if (validation.firstPlayerEnabled) fprintf(stderr, "Repaired golf round: enabled player slot 0 on Blue tee\n");
  if (validation.currentHoleReset) fprintf(stderr, "Repaired golf round: currentHole reset to 0\n");
  if (validation.currentPlayerReset) fprintf(stderr, "Repaired golf round: currentPlayer reset to first enabled\n");
  for (uint8_t slot = 0; slot < GolfRound::MAX_PLAYERS; ++slot) {
    const GolfPlayerValidationResult& player = validation.players[slot];
    for (uint8_t hole = 0; hole < GolfRound::MAX_HOLES; ++hole) {
      if ((player.puttsRepaired | player.in100Repaired | player.penaltyCountRepaired |
           player.penaltyEventRepaired | player.penaltyMarkerRepaired) & (1UL << hole)) {
        fprintf(stderr, "Repaired golf round: player %u hole %u\n", slot, static_cast<unsigned>(hole + 1));
      }
    }
  }
}

}  // namespace

bool RoundStore::write(const GolfRound& round) { return atomicWrite(&round, nullptr); }

GolfJsonResult RoundStore::read(GolfRound& round) {
  char path[GOLF_PATH_CAPACITY];
  if (!golfPath(path, sizeof(path), GOLF_STATE_FILE)) return {GolfJsonStatus::IoError, {}, {}};
  FILE* file = fopen(path, "rb");
  if (file == nullptr) return {GolfJsonStatus::IoError, {}, {}};
  if (fseek(file, 0, SEEK_END) != 0) {
    fclose(file);
    return {GolfJsonStatus::IoError, {}, {}};
  }
  const long length = ftell(file);
  if (length < 0 || length > 65535 || fseek(file, 0, SEEK_SET) != 0) {
    fclose(file);
    return {GolfJsonStatus::InvalidJson, {}, {}};
  }
  char* json = static_cast<char*>(malloc(static_cast<size_t>(length) + 1));
  if (json == nullptr) {
    fclose(file);
    return {GolfJsonStatus::IoError, {}, {}};
  }
  const bool read = fread(json, 1, static_cast<size_t>(length), file) == static_cast<size_t>(length);
  fclose(file);
  json[length] = '\0';
  GolfJsonResult result = read ? golfReadRoundJson(json, static_cast<size_t>(length), true, round)
                               : GolfJsonResult{GolfJsonStatus::IoError, {}, {}};
  free(json);
  if (result.status == GolfJsonStatus::Archived) {
    if ((unlink(path) != 0 && errno != ENOENT) || !syncParent(path)) result.status = GolfJsonStatus::IoError;
  } else if (result.status == GolfJsonStatus::Ok) {
    logRepairs(result.validation);
  }
  return result;
}

bool RoundStore::writeArchiveMarker(const char* filename) {
  return filename != nullptr && filename[0] != '\0' && strchr(filename, '/') == nullptr &&
         strlen(filename) < GOLF_ARCHIVE_NAME_CAPACITY && atomicWrite(nullptr, filename);
}

bool RoundStore::clear() {
  char path[GOLF_PATH_CAPACITY];
  if (!golfPath(path, sizeof(path), GOLF_STATE_FILE)) return false;
  if (unlink(path) == 0) return syncParent(path);
  return errno == ENOENT;
}
