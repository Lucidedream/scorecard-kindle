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

bool validArchiveName(const char* file) {
  return file != nullptr && file[0] != '\0' && strchr(file, '/') == nullptr && strstr(file, "..") == nullptr;
}

bool lineMatchesFile(const char* line, const char* filename, int* slot = nullptr) {
  // The archive filename is generated and therefore never quoted. It is the final field.
  const char* end = line + strlen(line);
  while (end > line && (end[-1] == '\n' || end[-1] == '\r')) --end;
  const char* comma = end;
  while (comma > line && comma[-1] != ',') --comma;
  if (static_cast<size_t>(end - comma) != strlen(filename) || strncmp(comma, filename, end - comma) != 0) return false;
  if (slot != nullptr) {
    // Find playerSlot (field 3), respecting the only potentially quoted preceding field: course.
    unsigned field = 0;
    bool quoted = false;
    const char* start = line;
    for (const char* p = line; p < comma; ++p) {
      if (*p == '"') {
        if (quoted && p + 1 < comma && p[1] == '"') ++p;
        else quoted = !quoted;
      } else if (*p == ',' && !quoted) {
        if (field == 3) break;
        ++field;
        start = p + 1;
      }
    }
    if (field != 3) return false;
    char* parsedEnd = nullptr;
    const long parsed = strtol(start, &parsedEnd, 10);
    if (parsedEnd == start || *parsedEnd != ',') return false;
    *slot = static_cast<int>(parsed);
  }
  return true;
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

bool writeIndexRow(FILE* output, const char* filename, const GolfRound& round, const uint8_t slot) {
  const GolfPlayer& player = round.players[slot];
  char date[11] = "";
  golfFormatDate(round.dateYmd, date, sizeof(date));
  return csv(output, date) && fputc(',', output) != EOF && csv(output, round.courseName) &&
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

  for (uint8_t slot = 0; ok && slot < GolfRound::MAX_PLAYERS; ++slot) {
    const GolfPlayer& player = round.players[slot];
    if (!golfPlayerIsEnabled(player)) continue;
    ok = writeIndexRow(output, filename, round, slot);
  }
  ok = ok && syncFile(output);
  if (fclose(output) != 0) ok = false;
  if (!ok || rename(staged, live) != 0) {
    unlink(staged);
    return false;
  }
  return syncDirectory(roundsPath);
}

bool stageIndexWithoutFile(const char* roundsPath, const char* filename, const GolfRound* replacement,
                           size_t& originalRows, size_t& stagedRows, char* staged, const size_t stagedCapacity,
                           char* live, const size_t liveCapacity) {
  if (snprintf(live, liveCapacity, "%s/%s", roundsPath, GOLF_INDEX_FILE) >= static_cast<int>(liveCapacity) ||
      snprintf(staged, stagedCapacity, "%s.new", live) >= static_cast<int>(stagedCapacity)) return false;
  FILE* input = fopen(live, "rb");
  FILE* output = fopen(staged, "wb");
  if (input == nullptr || output == nullptr) {
    if (input != nullptr) fclose(input);
    if (output != nullptr) fclose(output);
    unlink(staged);
    return false;
  }
  char line[1024];
  bool ok = fgets(line, sizeof(line), input) != nullptr && strcmp(line, INDEX_HEADER) == 0 &&
            fputs(INDEX_HEADER, output) != EOF;
  originalRows = 0;
  stagedRows = 0;
  while (ok && fgets(line, sizeof(line), input) != nullptr) {
    ++originalRows;
    if (lineMatchesFile(line, filename)) continue;
    ok = fputs(line, output) != EOF;
    if (ok) ++stagedRows;
  }
  if (ferror(input)) ok = false;
  if (replacement != nullptr) {
    for (uint8_t slot = 0; ok && slot < GolfRound::MAX_PLAYERS; ++slot) {
      if (!golfPlayerIsEnabled(replacement->players[slot])) continue;
      ok = writeIndexRow(output, filename, *replacement, slot);
      if (ok) ++stagedRows;
    }
  }
  ok = ok && syncFile(output);
  fclose(input);
  if (fclose(output) != 0) ok = false;
  if (!ok) unlink(staged);
  return ok;
}

bool loadArchive(const char* path, GolfRound& round) {
  FILE* input = fopen(path, "rb");
  if (input == nullptr || fseek(input, 0, SEEK_END) != 0) { if (input != nullptr) fclose(input); return false; }
  const long length = ftell(input);
  if (length <= 0 || length > 1024 * 1024 || fseek(input, 0, SEEK_SET) != 0) { fclose(input); return false; }
  char* json = static_cast<char*>(malloc(static_cast<size_t>(length)));
  if (json == nullptr) { fclose(input); return false; }
  const bool read = fread(json, 1, static_cast<size_t>(length), input) == static_cast<size_t>(length);
  fclose(input);
  const bool ok = read && golfReadRoundJson(json, static_cast<size_t>(length), false, round).status == GolfJsonStatus::Ok;
  free(json);
  return ok;
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

bool removeRound(const char* file) {
  if (!validArchiveName(file)) return false;
  char roundsPath[GOLF_PATH_CAPACITY], archivePath[GOLF_PATH_CAPACITY];
  char staged[GOLF_PATH_CAPACITY], live[GOLF_PATH_CAPACITY];
  if (!golfPath(roundsPath, sizeof(roundsPath), GOLF_ROUNDS_DIR) ||
      snprintf(archivePath, sizeof(archivePath), "%s/%s", roundsPath, file) >= static_cast<int>(sizeof(archivePath))) return false;
  size_t original = 0, remaining = 0;
  if (!stageIndexWithoutFile(roundsPath, file, nullptr, original, remaining, staged, sizeof(staged), live, sizeof(live)) ||
      original == remaining || getenv("GOLF_ARCHIVE_FAIL_BEFORE_INDEX_RENAME") != nullptr) {
    unlink(staged);
    return false;
  }
  if (unlink(archivePath) != 0 && errno != ENOENT) { unlink(staged); return false; }
  if (rename(staged, live) != 0) { unlink(staged); return false; }
  return syncDirectory(roundsPath);
}

bool removePlayerFromRound(const char* file, const uint8_t playerSlot) {
  if (!validArchiveName(file) || playerSlot >= GolfRound::MAX_PLAYERS) return false;
  char roundsPath[GOLF_PATH_CAPACITY], archivePath[GOLF_PATH_CAPACITY];
  char staged[GOLF_PATH_CAPACITY], live[GOLF_PATH_CAPACITY];
  if (!golfPath(roundsPath, sizeof(roundsPath), GOLF_ROUNDS_DIR) ||
      snprintf(archivePath, sizeof(archivePath), "%s/%s", roundsPath, file) >= static_cast<int>(sizeof(archivePath))) return false;
  GolfRound round{};
  if (!loadArchive(archivePath, round) || !golfDisablePlayer(round, playerSlot) || golfEnabledPlayerCount(round) == 0) return false;
  if (!golfPlayerIsEnabled(round.players[round.currentPlayer])) {
    for (uint8_t slot = 0; slot < GolfRound::MAX_PLAYERS; ++slot) {
      if (golfPlayerIsEnabled(round.players[slot])) { round.currentPlayer = slot; break; }
    }
  }
  size_t original = 0, remaining = 0;
  if (!stageIndexWithoutFile(roundsPath, file, &round, original, remaining, staged, sizeof(staged), live, sizeof(live)) ||
      remaining + 1 != original || getenv("GOLF_ARCHIVE_FAIL_BEFORE_INDEX_RENAME") != nullptr) {
    unlink(staged);
    return false;
  }
  if (!writeArchiveFile(archivePath, round) || rename(staged, live) != 0) { unlink(staged); return false; }
  return syncDirectory(roundsPath);
}
