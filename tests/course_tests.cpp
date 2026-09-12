#include <assert.h>
#include <string.h>

#include "core/Course.h"

namespace {

void testCourseFiles() {
  Course courses[8]{};
  const uint8_t count = golfLoadSdCourses(courses, 8);
  assert(count == 3);
  assert(strcmp(courses[0].name, "Alpine One") == 0);
  assert(strcmp(courses[1].name, "Pebble Beach") == 0);
  assert(strcmp(courses[2].name, "Zephyr Links") == 0);

  assert(courses[0].holeCount == 18 && courses[0].teeCount == 1 && !courses[0].hasSi);
  assert(courses[0].par[0] == 3 && courses[0].par[2] == 5);
  assert(strcmp(courses[0].tees[0].name, "Gold") == 0 && courses[0].tees[0].yards[17] == 218);
  assert(courses[1].par[0] == 4 && strcmp(courses[1].tees[0].name, "Red") == 0);
  assert(courses[2].teeCount == 2 && courses[2].hasSi && courses[2].si[17] == 18);
  assert(strcmp(courses[2].tees[1].name, "White") == 0 && courses[2].tees[1].yards[0] == 351);

  bool builtInCollisionStillPresent = false;
  for (uint8_t index = 0; index < GOLF_BUILT_IN_COURSE_COUNT; ++index)
    if (strcmp(GOLF_BUILT_IN_COURSES[index].name, courses[1].name) == 0) builtInCollisionStillPresent = true;
  assert(builtInCollisionStillPresent);
}

void testCapacity() {
  Course courses[2]{};
  assert(golfLoadSdCourses(courses, 2) == 2);
  assert(strcmp(courses[0].name, "Alpine One") == 0);
  assert(strcmp(courses[1].name, "Pebble Beach") == 0);
  assert(golfLoadSdCourses(nullptr, 0) == 0);
}

}  // namespace

int main() {
  testCourseFiles();
  testCapacity();
  return 0;
}
