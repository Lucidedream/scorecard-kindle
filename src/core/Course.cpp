#include "Course.h"

#include <string.h>

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
