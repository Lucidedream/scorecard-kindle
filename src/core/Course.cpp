#include "Course.h"

#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "store/GolfPaths.h"

namespace {

constexpr size_t MAX_COURSE_FILE_SIZE = 64 * 1024;

class CourseReader {
 public:
  CourseReader(const char* data, const size_t size) : current_(data), end_(data + size) {}

  bool finish() { space(); return current_ == end_; }

  bool token(const char* value) {
    space();
    const size_t length = strlen(value);
    if (static_cast<size_t>(end_ - current_) < length || memcmp(current_, value, length) != 0) return false;
    current_ += length;
    return true;
  }

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

  bool string(char* output, const size_t capacity, bool& truncated) {
    space();
    if (current_ == end_ || *current_++ != '"' || capacity == 0) return false;
    size_t used = 0;
    truncated = false;
    while (current_ != end_ && *current_ != '"') {
      unsigned char value = static_cast<unsigned char>(*current_++);
      if (value < 0x20) return false;
      if (value == '\\') {
        if (current_ == end_) return false;
        switch (*current_++) {
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
      if (used + 1 < capacity) output[used++] = static_cast<char>(value);
      else truncated = true;
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

template <typename T>
bool readNumbers(CourseReader& reader, T* output, const unsigned minimum, const unsigned maximum) {
  if (!reader.token("[")) return false;
  for (uint8_t index = 0; index < GolfRound::MAX_HOLES; ++index) {
    unsigned value = 0;
    if ((index != 0 && !reader.token(",")) || !reader.number(value, maximum) || value < minimum) return false;
    output[index] = static_cast<T>(value);
  }
  return reader.token("]");
}

bool nameValid(const char* name, const bool commaAllowed) {
  if (name[0] == '\0') return false;
  for (const unsigned char* byte = reinterpret_cast<const unsigned char*>(name); *byte != '\0'; ++byte)
    if (*byte < 0x20 || (!commaAllowed && *byte == ',')) return false;
  return true;
}

bool readTee(CourseReader& reader, CourseTee& tee) {
  if (!reader.token("{")) return false;
  bool hasName = false;
  bool hasYards = false;
  for (uint8_t member = 0;; ++member) {
    if (reader.token("}")) break;
    if (member != 0 && !reader.token(",")) return false;
    char key[16];
    bool truncated = false;
    if (!reader.string(key, sizeof(key), truncated) || truncated || !reader.token(":")) return false;
    if (strcmp(key, "name") == 0 && !hasName) {
      if (!reader.string(tee.name, sizeof(tee.name), truncated) || truncated || !nameValid(tee.name, false))
        return false;
      hasName = true;
    } else if (strcmp(key, "yards") == 0 && !hasYards) {
      if (!readNumbers(reader, tee.yards, 0, UINT16_MAX)) return false;
      hasYards = true;
    } else return false;
  }
  return hasName && hasYards;
}

bool readTees(CourseReader& reader, Course& course) {
  if (!reader.token("[")) return false;
  course.teeCount = 0;
  if (reader.token("]")) return false;
  for (;;) {
    if (course.teeCount >= 2 || !readTee(reader, course.tees[course.teeCount])) return false;
    ++course.teeCount;
    if (reader.token("]")) return true;
    if (!reader.token(",")) return false;
  }
}

bool readCourse(const char* data, const size_t size, Course& course) {
  course = {};
  CourseReader reader(data, size);
  if (!reader.token("{")) return false;
  bool hasName = false;
  bool hasHoles = false;
  bool hasPar = false;
  bool hasTees = false;
  for (uint8_t member = 0;; ++member) {
    if (reader.token("}")) break;
    if (member != 0 && !reader.token(",")) return false;
    char key[16];
    bool truncated = false;
    if (!reader.string(key, sizeof(key), truncated) || truncated || !reader.token(":")) return false;
    if (strcmp(key, "name") == 0 && !hasName) {
      if (!reader.string(course.name, sizeof(course.name), truncated) || truncated || !nameValid(course.name, true))
        return false;
      hasName = true;
    } else if (strcmp(key, "holes") == 0 && !hasHoles) {
      unsigned holes = 0;
      if (!reader.number(holes, UINT8_MAX) || holes != GolfRound::MAX_HOLES) return false;
      course.holeCount = static_cast<uint8_t>(holes);
      hasHoles = true;
    } else if (strcmp(key, "par") == 0 && !hasPar) {
      if (!readNumbers(reader, course.par, 3, 5)) return false;
      hasPar = true;
    } else if (strcmp(key, "si") == 0 && !course.hasSi) {
      if (!readNumbers(reader, course.si, 1, GolfRound::MAX_HOLES)) return false;
      course.hasSi = true;
    } else if (strcmp(key, "tees") == 0 && !hasTees) {
      if (!readTees(reader, course)) return false;
      hasTees = true;
    } else return false;
  }
  return reader.finish() && hasName && hasHoles && hasPar && hasTees;
}

int courseNameCompare(const char* left, const char* right) {
  while (*left != '\0' && *right != '\0') {
    const int difference = tolower(static_cast<unsigned char>(*left)) -
                           tolower(static_cast<unsigned char>(*right));
    if (difference != 0) return difference;
    ++left;
    ++right;
  }
  return static_cast<unsigned char>(*left) - static_cast<unsigned char>(*right);
}

void courseLog(const char* file, const char* reason) {
  fprintf(stderr, "Course %s skipped: %s\n", file, reason);
}

bool jsonFilename(const char* name) {
  const size_t length = strlen(name);
  return length > 5 && strcmp(name + length - 5, ".json") == 0;
}

bool loadCourseFile(const char* directory, const char* name, Course& course) {
  char path[GOLF_PATH_CAPACITY];
  const int length = snprintf(path, sizeof(path), "%s/%s", directory, name);
  if (length < 0 || static_cast<size_t>(length) >= sizeof(path)) return false;
  FILE* file = fopen(path, "rb");
  if (file == nullptr || fseek(file, 0, SEEK_END) != 0) {
    if (file != nullptr) fclose(file);
    return false;
  }
  const long fileSize = ftell(file);
  if (fileSize < 0 || static_cast<size_t>(fileSize) > MAX_COURSE_FILE_SIZE ||
      fseek(file, 0, SEEK_SET) != 0) {
    fclose(file);
    return false;
  }
  char* data = static_cast<char*>(malloc(static_cast<size_t>(fileSize) + 1));
  if (data == nullptr) { fclose(file); return false; }
  const size_t read = fread(data, 1, static_cast<size_t>(fileSize), file);
  const bool closed = fclose(file) == 0;
  data[read] = '\0';
  const bool valid = closed && read == static_cast<size_t>(fileSize) && readCourse(data, read, course);
  free(data);
  return valid;
}

}  // namespace

extern constexpr Course GOLF_BUILT_IN_COURSES[] = {
    {"Sanyang Golf Club",
     18,
     {4, 5, 3, 4, 5, 3, 4, 4, 4, 4, 4, 5, 3, 4, 4, 4, 3, 5},
     {325, 510, 144, 290, 510, 170, 427, 430, 390, 400, 395, 520, 175, 365, 375, 385, 165, 490},
     {15, 7, 13, 11, 9, 17, 3, 1, 5, 6, 12, 18, 16, 10, 14, 2, 4, 8},
     true,
     {{"Blue", {325, 510, 144, 290, 510, 170, 427, 430, 390, 400, 395, 520, 175, 365, 375, 385, 165, 490}},
      {"White", {310, 470, 122, 265, 490, 153, 389, 371, 340, 360, 360, 495, 150, 332, 350, 370, 156, 470}}},
     2},
    {"Pebble Beach",
     18,
     {4, 5, 4, 4, 3, 5, 3, 4, 4, 4, 4, 3, 4, 5, 4, 4, 3, 5},
     {},
     {7, 11, 3, 15, 17, 9, 13, 1, 5, 6, 12, 16, 4, 2, 8, 10, 18, 14},
     true,
     {{"Blue", {}}, {}},
     1},
    {"Template course", 18, {}, {}, {}, false, {{"Blue", {}}, {}}, 1},
};

extern constexpr uint8_t GOLF_BUILT_IN_COURSE_COUNT =
    static_cast<uint8_t>(sizeof(GOLF_BUILT_IN_COURSES) / sizeof(GOLF_BUILT_IN_COURSES[0]));

uint8_t golfLoadSdCourses(Course* output, const uint8_t capacity) {
  if (output == nullptr || capacity == 0) return 0;
  char path[GOLF_PATH_CAPACITY];
  if (!golfPath(path, sizeof(path), GOLF_COURSES_DIR)) return 0;
  DIR* directory = opendir(path);
  if (directory == nullptr) return 0;
  uint8_t count = 0;
  for (dirent* entry = readdir(directory); entry != nullptr; entry = readdir(directory)) {
    if (!jsonFilename(entry->d_name)) continue;
    Course course{};
    if (!loadCourseFile(path, entry->d_name, course)) {
      courseLog(entry->d_name, "invalid JSON or course schema");
      continue;
    }
    uint8_t position = 0;
    while (position < count && courseNameCompare(output[position].name, course.name) <= 0) ++position;
    if (count < capacity) {
      for (uint8_t index = count; index > position; --index) output[index] = output[index - 1];
      output[position] = course;
      ++count;
    } else if (capacity != 0 && position < capacity) {
      courseLog(output[capacity - 1].name, "no library space");
      for (uint8_t index = capacity - 1; index > position; --index) output[index] = output[index - 1];
      output[position] = course;
    } else courseLog(entry->d_name, "no library space");
  }
  closedir(directory);
  return count;
}

bool applyCourse(GolfRound& round, const Course& course, const char* tee) {
  if (course.holeCount != 18 || tee == nullptr || tee[0] == '\0') return false;
  const CourseTee* selected = nullptr;
  for (uint8_t index = 0; index < course.teeCount && index < 2; ++index) {
    if (strcmp(course.tees[index].name, tee) == 0) selected = &course.tees[index];
  }
  if (selected == nullptr) return false;

  round = {};
  initializeGolfPlayerDefaults(round);
  memcpy(round.courseName, course.name, sizeof(round.courseName));
  round.courseName[sizeof(round.courseName) - 1] = '\0';
  round.holeCount = course.holeCount;
  memcpy(round.par, course.par, sizeof(round.par));
  round.hasSi = course.hasSi;
  if (course.hasSi) memcpy(round.si, course.si, sizeof(round.si));
  golfSetTee(round.players[0], tee);
  memcpy(round.players[0].yards, selected->yards, sizeof(round.players[0].yards));
  round.currentPlayer = 0;
  return true;
}
