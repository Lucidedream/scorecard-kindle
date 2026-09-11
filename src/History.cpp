#include "History.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "store/GolfJson.h"

namespace {

bool parseUnsigned(const char* value, unsigned maximum, unsigned& output) {
  if (value == nullptr || *value == '\0') return false;
  char* end = nullptr;
  errno = 0;
  const unsigned long parsed = strtoul(value, &end, 10);
  if (errno != 0 || *end != '\0' || parsed > maximum) return false;
  output = static_cast<unsigned>(parsed);
  return true;
}

// Parses one physical CSV record. Archive fields may contain commas, quotes, CR, but
// archive-generated course and player names never contain embedded newlines.
bool csvFields(char* line, char** fields, const size_t capacity, size_t& count) {
  count = 0;
  char* read = line;
  char* write = line;
  while (*read != '\0') {
    if (count == capacity) return false;
    fields[count++] = write;
    bool quoted = false;
    if (*read == '"') {
      quoted = true;
      ++read;
    }
    while (*read != '\0') {
      if (quoted) {
        if (*read == '"' && read[1] == '"') {
          *write++ = '"';
          read += 2;
        } else if (*read == '"') {
          ++read;
          quoted = false;
          if (*read != ',' && *read != '\r' && *read != '\n' && *read != '\0') return false;
        } else {
          *write++ = *read++;
        }
      } else if (*read == ',') {
        ++read;
        break;
      } else if (*read == '\r' || *read == '\n') {
        while (*read == '\r' || *read == '\n') ++read;
        break;
      } else if (*read == '"') {
        return false;
      } else {
        *write++ = *read++;
      }
    }
    if (quoted) return false;
    *write++ = '\0';
  }
  return count != 0;
}

bool parseRow(char* line, HistoryRow& row) {
  char* fields[17];
  size_t count = 0;
  if (!csvFields(line, fields, 17, count) || count != 17) return false;
  unsigned holes = 0, slot = 0, strokes = 0, par = 0, putts = 0, in100 = 0, out100 = 0;
  unsigned hazards = 0, obs = 0, fairways = 0, fairwayHoles = 0, gir = 0, girHoles = 0;
  if (!parseUnsigned(fields[2], 18, holes) || (holes != 9 && holes != 18) ||
      !parseUnsigned(fields[3], GolfRound::MAX_PLAYERS - 1, slot) ||
      !parseUnsigned(fields[5], 65535, strokes) || !parseUnsigned(fields[6], 65535, par) ||
      !parseUnsigned(fields[7], 65535, putts) || !parseUnsigned(fields[8], 65535, in100) ||
      !parseUnsigned(fields[9], 65535, out100) || !parseUnsigned(fields[10], 65535, hazards) ||
      !parseUnsigned(fields[11], 65535, obs) || !parseUnsigned(fields[12], 65535, fairways) ||
      !parseUnsigned(fields[13], 65535, fairwayHoles) || !parseUnsigned(fields[14], 65535, gir) ||
      !parseUnsigned(fields[15], 65535, girHoles) || fields[16][0] == '\0') return false;
  row = {};
  if (strcmp(fields[0], "null") != 0) snprintf(row.date, sizeof(row.date), "%s", fields[0]);
  snprintf(row.course, sizeof(row.course), "%s", fields[1]);
  row.holes = static_cast<uint8_t>(holes);
  row.playerSlot = static_cast<uint8_t>(slot);
  snprintf(row.playerName, sizeof(row.playerName), "%s", fields[4]);
  row.strokes = static_cast<uint16_t>(strokes);
  row.par = static_cast<uint16_t>(par);
  row.putts = static_cast<uint16_t>(putts);
  row.in100 = static_cast<uint16_t>(in100);
  row.out100 = static_cast<uint16_t>(out100);
  row.hazards = static_cast<uint16_t>(hazards);
  row.obs = static_cast<uint16_t>(obs);
  row.fairways = static_cast<uint16_t>(fairways);
  row.fairwayHoles = static_cast<uint16_t>(fairwayHoles);
  row.gir = static_cast<uint16_t>(gir);
  row.girHoles = static_cast<uint16_t>(girHoles);
  snprintf(row.file, sizeof(row.file), "%s", fields[16]);
  return true;
}

}  // namespace

HistoryReadResult golfReadHistoryIndex(HistoryRow* rows, const size_t capacity) {
  HistoryReadResult result{};
  if (rows == nullptr || capacity == 0) return result;
  char relative[GOLF_PATH_CAPACITY];
  char path[GOLF_PATH_CAPACITY];
  if (snprintf(relative, sizeof(relative), "%s/%s", GOLF_ROUNDS_DIR, GOLF_INDEX_FILE) >=
          static_cast<int>(sizeof(relative)) ||
      !golfPath(path, sizeof(path), relative)) return result;
  FILE* input = fopen(path, "rb");
  if (input == nullptr) {
    result.readable = errno == ENOENT;
    return result;
  }
  char line[1024];
  if (fgets(line, sizeof(line), input) == nullptr || strncmp(line, "date,course,holes,playerSlot,", 29) != 0) {
    fclose(input);
    return result;
  }
  result.readable = true;
  size_t accepted = 0;
  while (fgets(line, sizeof(line), input) != nullptr) {
    if (strchr(line, '\n') == nullptr && !feof(input)) {
      int ch = 0;
      while ((ch = fgetc(input)) != '\n' && ch != EOF) {}
      continue;
    }
    HistoryRow row{};
    if (!parseRow(line, row)) continue;
    if (accepted < capacity) rows[accepted] = row;
    else rows[accepted % capacity] = row;
    ++accepted;
  }
  if (ferror(input)) result.readable = false;
  fclose(input);
  result.olderRoundsExist = accepted > capacity;
  result.count = accepted < capacity ? accepted : capacity;
  if (accepted > capacity) {
    const size_t start = accepted % capacity;
    for (size_t rotation = 0; rotation < start; ++rotation) {
      const HistoryRow first = rows[0];
      memmove(rows, rows + 1, (result.count - 1) * sizeof(HistoryRow));
      rows[result.count - 1] = first;
    }
  }
  return result;
}

size_t golfHistoryPlayers(const HistoryRow* rows, const size_t count, HistoryPlayer* players,
                          const size_t capacity) {
  if (rows == nullptr || players == nullptr) return 0;
  size_t found = 0;
  for (size_t i = 0; i < count; ++i) {
    size_t existing = found;
    for (size_t p = 0; p < found; ++p) if (players[p].slot == rows[i].playerSlot) existing = p;
    if (existing < found) {
      ++players[existing].roundCount;
      snprintf(players[existing].name, sizeof(players[existing].name), "%s", rows[i].playerName);
    } else if (found < capacity) {
      players[found] = {};
      players[found].slot = rows[i].playerSlot;
      players[found].roundCount = 1;
      snprintf(players[found].name, sizeof(players[found].name), "%s", rows[i].playerName);
      ++found;
    }
  }
  return found;
}

size_t golfHistoryRowsForPlayer(const HistoryRow* rows, const size_t count, const uint8_t slot,
                                const HistoryRow** matches, const size_t capacity) {
  if (rows == nullptr || matches == nullptr) return 0;
  size_t found = 0;
  for (size_t i = count; i-- > 0;) {
    if (rows[i].playerSlot == slot && found < capacity) matches[found++] = &rows[i];
  }
  return found;
}

size_t golfHistoryFileRowCount(const HistoryRow* rows, const size_t count, const char* file) {
  size_t found = 0;
  if (rows == nullptr || file == nullptr) return 0;
  for (size_t i = 0; i < count; ++i) if (strcmp(rows[i].file, file) == 0) ++found;
  return found;
}

bool golfReadHistoryRound(const char* file, GolfRound& round) {
  if (file == nullptr || *file == '\0' || strchr(file, '/') != nullptr || strstr(file, "..") != nullptr) return false;
  char relative[GOLF_PATH_CAPACITY];
  char path[GOLF_PATH_CAPACITY];
  if (snprintf(relative, sizeof(relative), "%s/%s", GOLF_ROUNDS_DIR, file) >= static_cast<int>(sizeof(relative)) ||
      !golfPath(path, sizeof(path), relative)) return false;
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

SummaryView golfHistorySummaryView(const HistoryRow& row) {
  SummaryView view{};
  view.hasPar = row.par != 0;
  view.playerSlot = row.playerSlot;
  view.playerCount = 1;
  snprintf(view.header, sizeof(view.header), "ROUND SUMMARY · %s", row.playerName);
  snprintf(view.player, sizeof(view.player), "%s", row.playerName);
  snprintf(view.score, sizeof(view.score), "%u", row.strokes);
  if (row.par != 0) {
    const int toPar = static_cast<int>(row.strokes) - static_cast<int>(row.par);
    if (toPar == 0) snprintf(view.toPar, sizeof(view.toPar), "E");
    else snprintf(view.toPar, sizeof(view.toPar), "%+d", toPar);
  }
  snprintf(view.putts, sizeof(view.putts), "%u", row.putts);
  snprintf(view.in100, sizeof(view.in100), "%u", row.in100);
  snprintf(view.longGame, sizeof(view.longGame), "%u", row.strokes >= row.in100 ? row.strokes - row.in100 : 0);
  snprintf(view.penalties, sizeof(view.penalties), "%u", row.hazards + 2U * row.obs);
  snprintf(view.fairways, sizeof(view.fairways), "%u/%u", row.fairways, row.fairwayHoles);
  snprintf(view.greens, sizeof(view.greens), "%u/%u", row.gir, row.girHoles);
  return view;
}
