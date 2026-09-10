#include "GolfJson.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "core/GolfPenalty.h"

namespace {

bool leapYear(const uint16_t year) { return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0); }

uint8_t daysInMonth(const uint16_t year, const uint8_t month) {
  static constexpr uint8_t days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  if (month == 2 && leapYear(year)) return 29;
  return month >= 1 && month <= 12 ? days[month - 1] : 0;
}

class Reader {
 public:
  Reader(const char* data, const size_t size) : current_(data), end_(data + size) {}

  bool finish() {
    space();
    return current_ == end_;
  }

  bool token(const char* value) {
    space();
    const size_t length = strlen(value);
    if (static_cast<size_t>(end_ - current_) < length || memcmp(current_, value, length) != 0) return false;
    current_ += length;
    return true;
  }

  bool member(char* name, const size_t capacity) { return string(name, capacity) && token(":"); }

  bool number(unsigned& value, const unsigned maximum) {
    space();
    if (current_ == end_ || !isdigit(static_cast<unsigned char>(*current_))) return false;
    unsigned result = 0;
    do {
      const unsigned digit = static_cast<unsigned>(*current_ - '0');
      if (result > (maximum - digit) / 10) return false;
      result = result * 10 + digit;
      ++current_;
    } while (current_ != end_ && isdigit(static_cast<unsigned char>(*current_)));
    value = result;
    return true;
  }

  bool boolean(bool& value) {
    if (token("true")) {
      value = true;
      return true;
    }
    if (token("false")) {
      value = false;
      return true;
    }
    return false;
  }

  bool string(char* output, const size_t capacity) {
    space();
    if (current_ == end_ || *current_++ != '"') return false;
    size_t used = 0;
    while (current_ != end_ && *current_ != '"') {
      unsigned char value = static_cast<unsigned char>(*current_++);
      if (value < 0x20) return false;
      if (value == '\\') {
        if (current_ == end_) return false;
        const char escape = *current_++;
        switch (escape) {
          case '"': value = '"'; break;
          case '\\': value = '\\'; break;
          case '/': value = '/'; break;
          case 'b': value = '\b'; break;
          case 'f': value = '\f'; break;
          case 'n': value = '\n'; break;
          case 'r': value = '\r'; break;
          case 't': value = '\t'; break;
          default: return false;
        }
      }
      if (used + 1 >= capacity) return false;
      output[used++] = static_cast<char>(value);
    }
    if (current_ == end_) return false;
    ++current_;
    output[used] = '\0';
    return true;
  }

 private:
  void space() {
    while (current_ != end_ && isspace(static_cast<unsigned char>(*current_))) ++current_;
  }

  const char* current_;
  const char* end_;
};

bool textValid(const char* text, const bool allowEmpty, const bool commaAllowed) {
  if (text == nullptr || (!allowEmpty && text[0] == '\0')) return false;
  if (strpbrk(text, "\r\n") != nullptr || (!commaAllowed && strchr(text, ',') != nullptr)) return false;
  const auto* current = reinterpret_cast<const uint8_t*>(text);
  while (*current != 0) {
    if (*current < 0x80) {
      ++current;
    } else if (*current >= 0xc2 && *current <= 0xdf && current[1] != 0 && (current[1] & 0xc0) == 0x80) {
      current += 2;
    } else if (*current >= 0xe0 && *current <= 0xef && current[1] != 0 && current[2] != 0 &&
               (current[1] & 0xc0) == 0x80 &&
               (current[2] & 0xc0) == 0x80 && !(*current == 0xe0 && current[1] < 0xa0) &&
               !(*current == 0xed && current[1] >= 0xa0)) {
      current += 3;
    } else if (*current >= 0xf0 && *current <= 0xf4 && current[1] != 0 && current[2] != 0 && current[3] != 0 &&
               (current[1] & 0xc0) == 0x80 &&
               (current[2] & 0xc0) == 0x80 && (current[3] & 0xc0) == 0x80 &&
               !(*current == 0xf0 && current[1] < 0x90) && !(*current == 0xf4 && current[1] >= 0x90)) {
      current += 4;
    } else {
      return false;
    }
  }
  return true;
}

bool comma(Reader& reader, const uint8_t index) { return index == 0 || reader.token(","); }

template <typename T>
bool readArray(Reader& reader, T* output, const uint8_t count, const unsigned maximum) {
  if (!reader.token("[")) return false;
  for (uint8_t index = 0; index < count; ++index) {
    unsigned value = 0;
    if (!comma(reader, index) || !reader.number(value, maximum)) return false;
    output[index] = static_cast<T>(value);
  }
  return reader.token("]");
}

void storePacked(GolfPlayerScore& score, const uint8_t hole, const uint8_t index, const uint8_t packed) {
  uint8_t& target = score.penaltyEvents[hole][index / 2];
  const uint8_t shift = index % 2 == 0 ? 0 : 4;
  target = static_cast<uint8_t>((target & ~(0x0f << shift)) | ((packed & 0x0f) << shift));
}

bool readPenalties(Reader& reader, GolfPlayerScore& score, const uint8_t holes) {
  if (!reader.token("[")) return false;
  for (uint8_t hole = 0; hole < holes; ++hole) {
    if (!comma(reader, hole) || !reader.token("[")) return false;
    uint8_t count = 0;
    if (!reader.token("]")) {
      for (;;) {
        if (count != 0 && !reader.token(",")) return false;
        unsigned field = 0;
        unsigned kind = 0;
        if (!reader.token("[") || !reader.number(field, 255) || !reader.token(",") ||
            !reader.number(kind, 255) || !reader.token("]")) {
          return false;
        }
        if (count < GolfRound::MAX_PENALTIES_PER_HOLE) {
          const uint8_t packed = field <= static_cast<unsigned>(GolfField::Out100) &&
                                         kind <= static_cast<unsigned>(GolfPenaltyKind::Ob)
                                     ? golfPackPenaltyEvent(static_cast<GolfField>(field),
                                                           static_cast<GolfPenaltyKind>(kind))
                                     : 0x08;
          storePacked(score, hole, count, packed);
        }
        if (count < UINT8_MAX) ++count;
        if (reader.token("]")) break;
      }
    }
    score.penaltyCount[hole] = count > GolfRound::MAX_PENALTIES_PER_HOLE
                                   ? GolfRound::MAX_PENALTIES_PER_HOLE + 1
                                   : count;
  }
  return reader.token("]");
}

bool readBits(Reader& reader, GolfPlayerScore& score, const uint8_t holes, const bool bunker) {
  uint8_t values[GolfRound::MAX_HOLES]{};
  if (!readArray(reader, values, holes, 1)) return false;
  for (uint8_t hole = 0; hole < holes; ++hole) {
    if (bunker) {
      golfSetGreensideBunker(score, hole, values[hole] != 0);
    } else {
      golfSetFairwayHit(score, hole, values[hole] != 0);
    }
  }
  return true;
}

bool readPlayer(Reader& reader, GolfPlayer& player, const uint8_t holes) {
  if (!reader.token("{")) return false;
  uint16_t fields = 0;
  bool first = true;
  while (!reader.token("}")) {
    char name[16];
    if ((!first && !reader.token(",")) || !reader.member(name, sizeof(name))) return false;
    first = false;
    uint16_t bit = 0;
    bool valid = false;
    if (strcmp(name, "name") == 0) {
      bit = 1U << 0;
      valid = reader.string(player.name, sizeof(player.name)) && textValid(player.name, false, true);
    } else if (strcmp(name, "tee") == 0) {
      bit = 1U << 1;
      valid = reader.string(player.tee, sizeof(player.tee)) && textValid(player.tee, true, false);
    } else if (strcmp(name, "yards") == 0) {
      bit = 1U << 2;
      valid = readArray(reader, player.yards, holes, UINT16_MAX);
    } else if (strcmp(name, "putts") == 0) {
      bit = 1U << 3;
      valid = readArray(reader, player.score.putts, holes, 99);
    } else if (strcmp(name, "in100") == 0) {
      bit = 1U << 4;
      valid = readArray(reader, player.score.in100, holes, 99);
    } else if (strcmp(name, "out100") == 0) {
      bit = 1U << 5;
      valid = readArray(reader, player.score.out100, holes, 99);
    } else if (strcmp(name, "fairways") == 0) {
      bit = 1U << 6;
      valid = readBits(reader, player.score, holes, false);
    } else if (strcmp(name, "bunkers") == 0) {
      bit = 1U << 7;
      valid = readBits(reader, player.score, holes, true);
    } else if (strcmp(name, "penalties") == 0) {
      bit = 1U << 8;
      valid = readPenalties(reader, player.score, holes);
    }
    if (!valid || (fields & bit) != 0) return false;
    fields |= bit;
  }
  return fields == 0x1ff;
}

bool readPlayers(Reader& reader, GolfRound& round) {
  if (!reader.token("[")) return false;
  for (uint8_t slot = 0; slot < GolfRound::MAX_PLAYERS; ++slot) {
    if (!comma(reader, slot) || !readPlayer(reader, round.players[slot], GolfRound::MAX_HOLES)) return false;
  }
  return reader.token("]");
}

bool writeString(FILE* file, const char* value) {
  if (fputc('"', file) == EOF) return false;
  for (const auto* current = reinterpret_cast<const unsigned char*>(value); *current != 0; ++current) {
    switch (*current) {
      case '"': if (fputs("\\\"", file) == EOF) return false; break;
      case '\\': if (fputs("\\\\", file) == EOF) return false; break;
      case '\b': if (fputs("\\b", file) == EOF) return false; break;
      case '\f': if (fputs("\\f", file) == EOF) return false; break;
      case '\n': if (fputs("\\n", file) == EOF) return false; break;
      case '\r': if (fputs("\\r", file) == EOF) return false; break;
      case '\t': if (fputs("\\t", file) == EOF) return false; break;
      default: if (fputc(*current, file) == EOF) return false;
    }
  }
  return fputc('"', file) != EOF;
}

template <typename T>
bool writeArray(FILE* file, const T* values, const uint8_t count, const bool zero) {
  if (fputc('[', file) == EOF) return false;
  for (uint8_t index = 0; index < count; ++index) {
    if (index != 0 && fputc(',', file) == EOF) return false;
    if (fprintf(file, "%u", zero ? 0U : static_cast<unsigned>(values[index])) < 0) return false;
  }
  return fputc(']', file) != EOF;
}

bool writeBits(FILE* file, const GolfPlayerScore& score, const uint8_t holes, const bool zero, const bool bunker) {
  if (fputc('[', file) == EOF) return false;
  for (uint8_t hole = 0; hole < holes; ++hole) {
    if (hole != 0 && fputc(',', file) == EOF) return false;
    const bool set = !zero && (bunker ? golfGreensideBunker(score, hole) : golfFairwayHit(score, hole));
    if (fputc(set ? '1' : '0', file) == EOF) return false;
  }
  return fputc(']', file) != EOF;
}

bool writePenalties(FILE* file, const GolfPlayerScore& score, const uint8_t holes, const bool zero) {
  if (fputc('[', file) == EOF) return false;
  for (uint8_t hole = 0; hole < holes; ++hole) {
    if ((hole != 0 && fputc(',', file) == EOF) || fputc('[', file) == EOF) return false;
    const uint8_t count = zero || score.penaltyCount[hole] > GolfRound::MAX_PENALTIES_PER_HOLE
                              ? 0
                              : score.penaltyCount[hole];
    for (uint8_t index = 0; index < count; ++index) {
      GolfPenaltyEvent event{};
      if (!golfPenaltyEventAt(score, hole, index, event)) continue;
      if (index != 0 && fputc(',', file) == EOF) return false;
      if (fprintf(file, "[%u,%u]", static_cast<unsigned>(event.field), static_cast<unsigned>(event.kind)) < 0)
        return false;
    }
    if (fputc(']', file) == EOF) return false;
  }
  return fputc(']', file) != EOF;
}

bool markerFirst(const char* json, const size_t size, char* marker, const size_t capacity, bool& present) {
  present = false;
  const char needle[] = "\"archivedAs\"";
  for (size_t index = 0; index + sizeof(needle) - 1 <= size; ++index) {
    if (memcmp(json + index, needle, sizeof(needle) - 1) != 0) continue;
    size_t escapes = 0;
    for (size_t before = index; before != 0 && json[before - 1] == '\\'; --before) ++escapes;
    if ((escapes & 1U) != 0) continue;
    const char* after = json + index + sizeof(needle) - 1;
    Reader reader(after, size - static_cast<size_t>(after - json));
    if (!reader.token(":")) continue;
    present = true;
    return reader.string(marker, capacity) && marker[0] != '\0' && strchr(marker, '/') == nullptr;
  }
  return true;
}

}  // namespace

bool golfFormatDate(const uint16_t dateYmd, char* output, const size_t capacity) {
  const unsigned year = 2000U + (dateYmd >> 9);
  const unsigned month = (dateYmd >> 5) & 15U;
  const unsigned day = dateYmd & 31U;
  if (dateYmd == 0 || day == 0 || day > daysInMonth(static_cast<uint16_t>(year), static_cast<uint8_t>(month)))
    return false;
  const int written = snprintf(output, capacity, "%04u-%02u-%02u", year, month, day);
  return written == 10 && capacity > 10;
}

bool golfParseDate(const char* date, uint16_t& dateYmd) {
  if (date == nullptr || strlen(date) != 10 || date[4] != '-' || date[7] != '-') return false;
  for (uint8_t index = 0; index < 10; ++index) {
    if (index != 4 && index != 7 && (date[index] < '0' || date[index] > '9')) return false;
  }
  const unsigned year = static_cast<unsigned>((date[0] - '0') * 1000 + (date[1] - '0') * 100 +
                                               (date[2] - '0') * 10 + date[3] - '0');
  const unsigned month = static_cast<unsigned>((date[5] - '0') * 10 + date[6] - '0');
  const unsigned day = static_cast<unsigned>((date[8] - '0') * 10 + date[9] - '0');
  if (year < 2000 || year > 2127 || day == 0 || day > daysInMonth(year, month)) return false;
  dateYmd = static_cast<uint16_t>(((year - 2000) << 9) | (month << 5) | day);
  return true;
}

bool golfWriteRoundJson(FILE* file, const GolfRound& round, const bool stateFile, const char* archivedAs) {
  if (file == nullptr) return false;
  if (archivedAs != nullptr) {
    return fputs("{\"archivedAs\":", file) != EOF && writeString(file, archivedAs) && fputs("}\n", file) != EOF;
  }
  if (round.holeCount != GolfRound::MAX_HOLES) return false;
  if (memchr(round.courseName, '\0', sizeof(round.courseName)) == nullptr ||
      !textValid(round.courseName, false, true)) return false;
  for (const GolfPlayer& player : round.players) {
    if (memchr(player.name, '\0', sizeof(player.name)) == nullptr ||
        memchr(player.tee, '\0', sizeof(player.tee)) == nullptr || !textValid(player.name, false, true) ||
        !textValid(player.tee, true, false)) return false;
  }
  char date[11];
  if (fputs("{\"v\":6,\"date\":", file) == EOF) return false;
  if (golfFormatDate(round.dateYmd, date, sizeof(date))) {
    if (!writeString(file, date)) return false;
  } else if (fputs("null", file) == EOF) {
    return false;
  }
  if (fputs(",\"course\":", file) == EOF || !writeString(file, round.courseName) ||
      fprintf(file, ",\"holes\":%u", round.holeCount) < 0) return false;
  if (stateFile && fprintf(file, ",\"currentHole\":%u,\"currentPlayer\":%u", round.currentHole,
                           round.currentPlayer) < 0) return false;
  if (fputs(",\"par\":", file) == EOF || !writeArray(file, round.par, round.holeCount, false) ||
      fprintf(file, ",\"hasSi\":%s,\"si\":", round.hasSi ? "true" : "false") < 0 ||
      !writeArray(file, round.si, round.holeCount, !round.hasSi) || fputs(",\"players\":[", file) == EOF)
    return false;
  for (uint8_t slot = 0; slot < GolfRound::MAX_PLAYERS; ++slot) {
    const GolfPlayer& player = round.players[slot];
    const bool zero = !golfPlayerIsEnabled(player);
    if (slot != 0 && fputc(',', file) == EOF) return false;
    if (fputs("{\"name\":", file) == EOF || !writeString(file, player.name) || fputs(",\"tee\":", file) == EOF ||
        !writeString(file, player.tee) || fputs(",\"yards\":", file) == EOF ||
        !writeArray(file, player.yards, round.holeCount, zero) || fputs(",\"putts\":", file) == EOF ||
        !writeArray(file, player.score.putts, round.holeCount, zero) || fputs(",\"in100\":", file) == EOF ||
        !writeArray(file, player.score.in100, round.holeCount, zero) || fputs(",\"out100\":", file) == EOF ||
        !writeArray(file, player.score.out100, round.holeCount, zero) || fputs(",\"fairways\":", file) == EOF ||
        !writeBits(file, player.score, round.holeCount, zero, false) || fputs(",\"bunkers\":", file) == EOF ||
        !writeBits(file, player.score, round.holeCount, zero, true) || fputs(",\"penalties\":", file) == EOF ||
        !writePenalties(file, player.score, round.holeCount, zero) || fputc('}', file) == EOF) return false;
  }
  return fputs("]}\n", file) != EOF;
}

GolfJsonResult golfReadRoundJson(const char* json, const size_t size, const bool stateFile, GolfRound& round) {
  GolfJsonResult result{GolfJsonStatus::InvalidJson, {}, {}};
  bool markerPresent = false;
  if (!markerFirst(json, size, result.archivedAs, sizeof(result.archivedAs), markerPresent)) return result;
  if (markerPresent) {
    result.status = GolfJsonStatus::Archived;
    return result;
  }

  auto* decoded = static_cast<GolfRound*>(calloc(1, sizeof(GolfRound)));
  if (decoded == nullptr) return {GolfJsonStatus::IoError, {}, {}};
  initializeGolfPlayerDefaults(*decoded);
  Reader reader(json, size);
  unsigned version = 0;
  unsigned holes = 0;
  unsigned currentHole = 0;
  unsigned currentPlayer = 0;
  uint16_t fields = 0;
  bool valid = reader.token("{");
  bool first = true;
  while (valid && !reader.token("}")) {
    char name[20];
    if ((!first && !reader.token(",")) || !reader.member(name, sizeof(name))) {
      valid = false;
      break;
    }
    first = false;
    uint16_t bit = 0;
    bool valueValid = false;
    if (strcmp(name, "v") == 0) {
      bit = 1U << 0;
      valueValid = reader.number(version, 255);
      if (valueValid && version != 6) {
        fprintf(stderr, "Rejected golf JSON v%u: only v6 is supported\n", version);
        free(decoded);
        result.status = GolfJsonStatus::RejectedVersion;
        return result;
      }
    } else if (strcmp(name, "date") == 0) {
      bit = 1U << 1;
      valueValid = reader.token("null");
      if (!valueValid) {
        char date[11];
        valueValid = reader.string(date, sizeof(date)) && golfParseDate(date, decoded->dateYmd);
      }
    } else if (strcmp(name, "course") == 0) {
      bit = 1U << 2;
      valueValid = reader.string(decoded->courseName, sizeof(decoded->courseName)) &&
                   textValid(decoded->courseName, false, true);
    } else if (strcmp(name, "holes") == 0) {
      bit = 1U << 3;
      valueValid = reader.number(holes, 255);
    } else if (strcmp(name, "currentHole") == 0 && stateFile) {
      bit = 1U << 4;
      valueValid = reader.number(currentHole, 255);
    } else if (strcmp(name, "currentPlayer") == 0 && stateFile) {
      bit = 1U << 5;
      valueValid = reader.number(currentPlayer, 255);
    } else if (strcmp(name, "par") == 0) {
      bit = 1U << 6;
      valueValid = readArray(reader, decoded->par, GolfRound::MAX_HOLES, UINT8_MAX);
    } else if (strcmp(name, "hasSi") == 0) {
      bit = 1U << 7;
      valueValid = reader.boolean(decoded->hasSi);
    } else if (strcmp(name, "si") == 0) {
      bit = 1U << 8;
      valueValid = readArray(reader, decoded->si, GolfRound::MAX_HOLES, UINT8_MAX);
    } else if (strcmp(name, "players") == 0) {
      bit = 1U << 9;
      valueValid = readPlayers(reader, *decoded);
    }
    if (!valueValid || bit == 0 || (fields & bit) != 0) valid = false;
    fields |= bit;
  }
  const uint16_t required = stateFile ? 0x3ff : 0x3cf;
  valid = valid && fields == required && reader.finish();
  if (!valid) {
    free(decoded);
    return result;
  }
  if (holes != GolfRound::MAX_HOLES) {
    free(decoded);
    result.status = GolfJsonStatus::InvalidMetadata;
    return result;
  }
  decoded->holeCount = 18;
  decoded->currentHole = static_cast<uint8_t>(currentHole);
  decoded->currentPlayer = static_cast<uint8_t>(currentPlayer);
  result.validation = validateGolfRound(*decoded);
  if (!result.validation.valid) {
    free(decoded);
    result.status = GolfJsonStatus::InvalidMetadata;
    return result;
  }
  round = *decoded;
  free(decoded);
  result.status = GolfJsonStatus::Ok;
  return result;
}
