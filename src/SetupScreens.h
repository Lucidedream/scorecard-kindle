#pragma once

#include <stdint.h>

#include "HitTester.h"
#include "PgmCanvas.h"
#include "core/Course.h"

enum class SetupScreen { Home, History, Courses, PlayerCount, Roster, EditPlayer, TeeList };

enum SetupAction {
  SetupNone = 100,
  SetupNewRound,
  SetupHistory,
  SetupBack,
  SetupCourseFirst,
  SetupCountMinus = SetupCourseFirst + 16,
  SetupCountPlus,
  SetupPrimary,
  SetupPlayerFirst,
  SetupEditName = SetupPlayerFirst + GOLF_MAX_PLAYERS,
  SetupEditTee,
  SetupTeeFirst,
};

struct HomeSummary {
  uint16_t rounds;
  bool hasLast;
  char lastCourse[40];
  uint16_t lastScore;
  int16_t lastToPar;
};

struct SetupState {
  const Course* course;
  uint8_t playerCount;
  uint8_t editPlayer;
  uint8_t teeIndex[GOLF_MAX_PLAYERS];
  char playerName[GOLF_MAX_PLAYERS][GolfPlayer::NAME_CAPACITY];
};

void initializeSetup(SetupState& setup);
uint8_t stepPlayerCount(uint8_t count, int direction);
const char* playerCountPrimaryLabel(uint8_t count);
bool playerCountSkipsRoster(uint8_t count);
void courseTeeSummary(const Course& course, char* output, uint16_t capacity);
void courseParLabel(const Course& course, char* output, uint16_t capacity);
uint16_t courseTeeYards(const Course& course, uint8_t teeIndex);
bool buildRoundFromSetup(const SetupState& setup, GolfRound& round);
bool readHomeSummary(HomeSummary& summary);

void drawSetupScreen(PgmCanvas& canvas, HitTester& hits, SetupScreen screen, const SetupState& setup,
                     const HomeSummary& summary);
