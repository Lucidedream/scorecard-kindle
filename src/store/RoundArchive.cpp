#include "RoundArchive.h"

#include <errno.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include "GolfJson.h"
#include "GolfPaths.h"
#include "RoundStore.h"
#include "core/GolfPenalty.h"
#include "core/GolfStats.h"
#include "core/GolfValidate.h"

namespace {

constexpr char INDEX_HEADER[] =
    "date,course,holes,playerSlot,playerName,strokes,par,putts,in100,out100,hazards,obs,fairways,"
    "fairwayHoles,gir,girHoles,file\n";

bool ensureDirectory(const char* path) { return mkdir(path, 0755) == 0 || errno == EEXIST; }

bool syncFile(FILE* file) { return fflush(file) == 0 && fsync(fileno(file)) == 0; }

bool syncDirectory(const char* path) {
  const int descriptor = open(path, O_RDONLY | O_DIRECTORY);
  if (descriptor < 0) return false;
  const bool ok = fsync(descriptor) == 0;
  close(descriptor);
  return ok;
}

bool csv(FILE* file, const char* value) {
  const bool quote = strpbrk(value, ",\"\r\n") != nullptr;
  if (quote && fputc('"', file) == EOF) return false;
  for (const char* current = value; *current != '\0'; ++current) {
    if (*current == '"' && fputc('"', file) == EOF) return false;
    if (fputc(*current, file) == EOF) return false;
  }
  return !quote || fputc('"', file) != EOF;
}

bool copy(FILE* input, FILE* output) {
  char buffer[1024];
  size_t count = 0;
  while ((count = fread(buffer, 1, sizeof(buffer), input)) != 0) {
    if (fwrite(buffer, 1, count, output) != count) return false;
  }
  return !ferror(input);
}

unsigned nextSequence(const char* roundsPath) {
  unsigned maximum = 0;
  DIR* directory = opendir(roundsPath);
  if (directory == nullptr) return 1;
  while (const dirent* entry = readdir(directory)) {
    unsigned sequence = 0;
    if (sscanf(entry->d_name, "round-%u-", &sequence) == 1 && sequence > maximum) maximum = sequence;
  }
  closedir(directory);
  return maximum + 1;
}

bool writeArchiveFile(const char* path, const GolfRound& round) {
  char temporary[GOLF_PATH_CAPACITY];
  if (snprintf(temporary, sizeof(temporary), "%s.tmp", path) >= static_cast<int>(sizeof(temporary))) return false;
  FILE* file = fopen(temporary, "wb");
  if (file == nullptr) return false;
  bool ok = golfWriteRoundJson(file, round, false) && syncFile(file);
  if (fclose(file) != 0) ok = false;
  if (!ok || rename(temporary, path) != 0) {
    unlink(temporary);
    return false;
  }
  return true;
}

bool appendIndex(const char* roundsPath, const char* filename, const GolfRound& round) {
  char live[GOLF_PATH_CAPACITY];
  char staged[GOLF_PATH_CAPACITY];
  if (snprintf(live, sizeof(live), "%s/%s", roundsPath, GOLF_INDEX_FILE) >= static_cast<int>(sizeof(live)) ||
      snprintf(staged, sizeof(staged), "%s.tmp", live) >= static_cast<int>(sizeof(staged))) return false;
  FILE* output = fopen(staged, "wb");
  if (output == nullptr) return false;
  FILE* input = fopen(live, "rb");
  bool ok = true;
  if (input != nullptr) {
    char header[sizeof(INDEX_HEADER)];
    ok = fgets(header, sizeof(header), input) != nullptr && strcmp(header, INDEX_HEADER) == 0;
    rewind(input);
    ok = ok && copy(input, output);
    fclose(input);
  } else {
    ok = fputs(INDEX_HEADER, output) != EOF;
  }

  char date[11] = "";
  golfFormatDate(round.dateYmd, date, sizeof(date));
  for (uint8_t slot = 0; ok && slot < GolfRound::MAX_PLAYERS; ++slot) {
    const GolfPlayer& player = round.players[slot];
    if (!golfPlayerIsEnabled(player)) continue;
    ok = csv(output, date) && fputc(',', output) != EOF && csv(output, round.courseName) &&
         fprintf(output, ",%u,%u,", round.holeCount, slot) >= 0 && csv(output, player.name) &&
         fprintf(output, ",%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,",
                 golfScore(round, player.score), golfParTotal(round, player.score),
                 golfPuttsTotal(round, player.score), golfIn100Total(round, player.score),
                 golfLongTotal(round, player.score), golfHazardsForRound(player.score, round.holeCount),
                 golfObsForRound(player.score, round.holeCount), golfFairwaysHit(round, player.score),
                 golfFairwaysEligible(round, player.score), golfGreensInRegulation(round, player.score),
                 golfGreensEligible(round, player.score)) >= 0 &&
         csv(output, filename) && fputc('\n', output) != EOF;
  }
  ok = ok && syncFile(output);
  if (fclose(output) != 0) ok = false;
  if (!ok || rename(staged, live) != 0) {
    unlink(staged);
    return false;
  }
  return syncDirectory(roundsPath);
}

}  // namespace

void golfSlug(const char* course, char* output, const size_t capacity) {
  if (output == nullptr || capacity == 0) return;
  size_t used = 0;
  bool pendingHyphen = false;
  if (course != nullptr) {
    for (const unsigned char* current = reinterpret_cast<const unsigned char*>(course); *current != 0; ++current) {
      if (isascii(*current) && isalnum(*current)) {
        if (pendingHyphen && used != 0 && used < 40 && used + 1 < capacity) output[used++] = '-';
        pendingHyphen = false;
        if (used < 40 && used + 1 < capacity) output[used++] = static_cast<char>(tolower(*current));
      } else {
        pendingHyphen = used != 0;
      }
    }
  }
  if (used == 0) {
    const char fallback[] = "course";
    const size_t length = sizeof(fallback) - 1 < capacity - 1 ? sizeof(fallback) - 1 : capacity - 1;
    memcpy(output, fallback, length);
    used = length;
  }
  output[used] = '\0';
}

RoundArchiveResult archiveGolfRound(const GolfRound& round, char* filename, const size_t filenameCapacity) {
  auto* checked = static_cast<GolfRound*>(malloc(sizeof(GolfRound)));
  if (checked == nullptr) return RoundArchiveResult::Failed;
  const GolfJsonResult existing = RoundStore::read(*checked);
  if (existing.archivedAs[0] != '\0') {
    if (filename != nullptr && filenameCapacity != 0) {
      snprintf(filename, filenameCapacity, "%s", existing.archivedAs);
    }
    const bool cleared = existing.status == GolfJsonStatus::Archived || RoundStore::clear();
    free(checked);
    return cleared ? RoundArchiveResult::Complete : RoundArchiveResult::Failed;
  }
  *checked = round;
  const GolfValidationResult validation = validateGolfRound(*checked);
  if (!validation.valid || checked->holeCount != 18 || golfEnabledPlayerCount(*checked) == 0) {
    free(checked);
    return RoundArchiveResult::Failed;
  }

  char roundsPath[GOLF_PATH_CAPACITY];
  char relative[GOLF_PATH_CAPACITY];
  char slug[41];
  golfSlug(checked->courseName, slug, sizeof(slug));
  if (!ensureDirectory(golfStorageRoot()) ||
      snprintf(relative, sizeof(relative), "%s", GOLF_ROUNDS_DIR) >= static_cast<int>(sizeof(relative)) ||
      !golfPath(roundsPath, sizeof(roundsPath), relative) || !ensureDirectory(roundsPath)) {
    free(checked);
    return RoundArchiveResult::Failed;
  }

  const unsigned sequence = nextSequence(roundsPath);
  char chosen[GOLF_ARCHIVE_NAME_CAPACITY];
  char path[GOLF_PATH_CAPACITY];
  unsigned suffix = 1;
  do {
    const int length = suffix == 1 ? snprintf(chosen, sizeof(chosen), "round-%04u-%s.json", sequence, slug)
                                   : snprintf(chosen, sizeof(chosen), "round-%04u-%s-%u.json", sequence, slug, suffix);
    if (length < 0 || static_cast<size_t>(length) >= sizeof(chosen) ||
        snprintf(path, sizeof(path), "%s/%s", roundsPath, chosen) >= static_cast<int>(sizeof(path))) {
      free(checked);
      return RoundArchiveResult::Failed;
    }
    ++suffix;
  } while (access(path, F_OK) == 0);

  if (!writeArchiveFile(path, *checked) || !syncDirectory(roundsPath) || !appendIndex(roundsPath, chosen, *checked) ||
      !RoundStore::writeArchiveMarker(chosen) || !RoundStore::clear()) {
    free(checked);
    return RoundArchiveResult::Failed;
  }
  if (filename != nullptr && filenameCapacity != 0) snprintf(filename, filenameCapacity, "%s", chosen);
  free(checked);
  return RoundArchiveResult::Complete;
}
