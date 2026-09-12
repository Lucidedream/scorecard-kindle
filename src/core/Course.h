#pragma once

#include <stdint.h>

#include "GolfRound.h"

struct CourseTee {
  char name[GOLF_TEE_CAPACITY];
  uint16_t yards[GolfRound::MAX_HOLES];
};

struct Course {
  char name[40];
  uint8_t holeCount;
  uint8_t par[GolfRound::MAX_HOLES];
  uint16_t yards[GolfRound::MAX_HOLES];
  uint8_t si[GolfRound::MAX_HOLES];
  bool hasSi;
  CourseTee tees[2];
  uint8_t teeCount;
};

extern const Course GOLF_BUILT_IN_COURSES[];
extern const uint8_t GOLF_BUILT_IN_COURSE_COUNT;

uint8_t golfLoadSdCourses(Course* output, uint8_t capacity);
bool applyCourse(GolfRound& round, const Course& course, const char* tee);
