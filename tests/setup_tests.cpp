#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "Keyboard.h"
#include "SetupScreens.h"
#include "core/GolfValidate.h"
#include "store/RoundStore.h"

namespace {

void testCourseFormatting() {
  char value[64];
  courseTeeSummary(GOLF_BUILT_IN_COURSES[0], value, sizeof(value));
  assert(strcmp(value, "Blue / White") == 0);
  courseParLabel(GOLF_BUILT_IN_COURSES[0], value, sizeof(value));
  assert(strcmp(value, "72") == 0);
  courseParLabel(GOLF_BUILT_IN_COURSES[2], value, sizeof(value));
  assert(strcmp(value, "-") == 0);
}

void testPlayerCount() {
  assert(stepPlayerCount(1, -1) == 1 && stepPlayerCount(1, 1) == 2 && stepPlayerCount(4, 1) == 4);
  assert(strcmp(playerCountPrimaryLabel(1), "START ROUND") == 0);
  assert(strcmp(playerCountPrimaryLabel(2), "NEXT") == 0);
  assert(playerCountSkipsRoster(1) && !playerCountSkipsRoster(3));
}

void testBuildRound(const uint8_t count) {
  SetupState setup{};
  initializeSetup(setup);
  setup.course = &GOLF_BUILT_IN_COURSES[0];
  setup.playerCount = count;
  setup.teeIndex[1] = 1;
  GolfRound round{};
  assert(buildRoundFromSetup(setup, round));
  assert(validateGolfRound(round).valid);
  assert(golfEnabledPlayerCount(round) == count);
  assert(round.currentHole == 0 && round.currentPlayer == 0);
  assert(strcmp(round.players[0].tee, "Blue") == 0 && round.players[0].yards[0] == 325);
  if (count == 3) {
    assert(strcmp(round.players[1].tee, "White") == 0 && round.players[1].yards[0] == 310);
    assert(strcmp(round.players[2].name, "Player 3") == 0);
  }
}

void testKeyboard() {
  KeyboardState keyboard{};
  initializeKeyboard(keyboard, "Noah");
  handleKeyboardAction(keyboard, KeyboardBackspace, TouchEvent::Kind::LongPress);
  handleKeyboardAction(keyboard, KeyboardShift, TouchEvent::Kind::Tap);
  handleKeyboardAction(keyboard, KeyboardLetterFirst, TouchEvent::Kind::Tap);
  handleKeyboardAction(keyboard, KeyboardLetterFirst + 1, TouchEvent::Kind::Tap);
  handleKeyboardAction(keyboard, KeyboardBackspace, TouchEvent::Kind::Tap);
  handleKeyboardAction(keyboard, KeyboardSpace, TouchEvent::Kind::Tap);
  handleKeyboardAction(keyboard, KeyboardLetterFirst + 2, TouchEvent::Kind::Tap);
  handleKeyboardAction(keyboard, KeyboardDone, TouchEvent::Kind::Tap);
  char output[24];
  assert(finishKeyboard(keyboard, output, sizeof(output)) && strcmp(output, "A c") == 0);

  initializeKeyboard(keyboard, "Old");
  handleKeyboardAction(keyboard, KeyboardBackspace, TouchEvent::Kind::LongPress);
  handleKeyboardAction(keyboard, KeyboardDone, TouchEvent::Kind::Tap);
  assert(!finishKeyboard(keyboard, output, sizeof(output)) && strcmp(output, "Old") == 0);
}

void testHomeSummary() {
  char root[] = "/tmp/scorecard-setup-tests.XXXXXX";
  assert(mkdtemp(root) != nullptr);
  assert(setenv("SCORECARD_DIR", root, 1) == 0);
  char rounds[256];
  snprintf(rounds, sizeof(rounds), "%s/rounds", root);
  assert(mkdir(rounds, 0755) == 0);
  char path[300];
  snprintf(path, sizeof(path), "%s/index.csv", rounds);
  FILE* file = fopen(path, "wb");
  assert(file != nullptr);
  fputs("date,course,holes,playerSlot,playerName,strokes,par,putts,in100,out100,hazards,obs,fairways,fairwayHoles,gir,girHoles,file\n", file);
  fputs("2026-09-09,Old Course,18,0,Noah,75,72,30,40,35,0,0,8,14,7,18,round-0001-old.json\n", file);
  fputs("2026-09-10,\"Newest, Course\",18,0,Noah,70,72,28,38,32,0,0,9,14,10,18,round-0002-new.json\n", file);
  fputs("2026-09-10,\"Newest, Course\",18,1,Player 2,74,72,31,40,34,0,0,7,14,8,18,round-0002-new.json\n", file);
  assert(fclose(file) == 0);
  HomeSummary summary{};
  assert(readHomeSummary(summary));
  assert(summary.rounds == 2 && strcmp(summary.lastCourse, "Newest, Course") == 0);
  assert(summary.lastScore == 74 && summary.lastToPar == 2);
  assert(unlink(path) == 0 && rmdir(rounds) == 0 && rmdir(root) == 0);
  unsetenv("SCORECARD_DIR");
}

void testSyntheticSetupWalks() {
  char root[] = "/tmp/scorecard-setup-walk.XXXXXX";
  assert(mkdtemp(root) != nullptr && setenv("SCORECARD_DIR", root, 1) == 0);
  SetupState setup{};
  initializeSetup(setup);
  setup.course = &GOLF_BUILT_IN_COURSES[0];
  assert(playerCountSkipsRoster(setup.playerCount));
  GolfRound round{};
  assert(buildRoundFromSetup(setup, round) && RoundStore::write(round));
  GolfRound loaded{};
  assert(RoundStore::read(loaded).status == GolfJsonStatus::Ok);
  assert(golfEnabledPlayerCount(loaded) == 1 && loaded.players[0].yards[0] == 325);

  initializeSetup(setup);
  setup.course = &GOLF_BUILT_IN_COURSES[0];
  setup.playerCount = stepPlayerCount(stepPlayerCount(setup.playerCount, 1), 1);
  assert(!playerCountSkipsRoster(setup.playerCount));
  KeyboardState keyboard{};
  initializeKeyboard(keyboard, setup.playerName[1]);
  handleKeyboardAction(keyboard, KeyboardBackspace, TouchEvent::Kind::LongPress);
  handleKeyboardAction(keyboard, KeyboardShift, TouchEvent::Kind::Tap);
  handleKeyboardAction(keyboard, KeyboardLetterFirst + ('S' - 'A'), TouchEvent::Kind::Tap);
  handleKeyboardAction(keyboard, KeyboardLetterFirst + ('A' - 'A'), TouchEvent::Kind::Tap);
  handleKeyboardAction(keyboard, KeyboardLetterFirst + ('M' - 'A'), TouchEvent::Kind::Tap);
  handleKeyboardAction(keyboard, KeyboardDone, TouchEvent::Kind::Tap);
  assert(finishKeyboard(keyboard, setup.playerName[1], sizeof(setup.playerName[1])));
  setup.teeIndex[1] = 1;
  assert(buildRoundFromSetup(setup, round) && RoundStore::write(round));
  assert(RoundStore::read(loaded).status == GolfJsonStatus::Ok);
  assert(golfEnabledPlayerCount(loaded) == 3 && strcmp(loaded.players[1].name, "Sam") == 0);
  assert(strcmp(loaded.players[1].tee, "White") == 0 && loaded.players[1].yards[0] == 310);
  assert(RoundStore::clear() && rmdir(root) == 0);
  unsetenv("SCORECARD_DIR");
}

void testSdCourseSetupWalk() {
  char root[] = "/tmp/scorecard-sd-setup-walk.XXXXXX";
  assert(mkdtemp(root) != nullptr && setenv("SCORECARD_DIR", root, 1) == 0);
  char coursesPath[256];
  snprintf(coursesPath, sizeof(coursesPath), "%s/courses", root);
  assert(mkdir(coursesPath, 0755) == 0);
  char fixturePath[300];
  snprintf(fixturePath, sizeof(fixturePath), "%s/home.json", coursesPath);
  FILE* fixture = fopen(fixturePath, "wb");
  assert(fixture != nullptr);
  fputs("{\"name\":\"SD Home\",\"holes\":18,"
        "\"par\":[3,4,5,4,3,4,5,4,4,3,4,5,4,3,4,5,4,4],"
        "\"tees\":[{\"name\":\"Blue\",\"yards\":[101,202,303,204,105,206,307,208,209,110,211,312,213,114,215,316,217,218]},"
        "{\"name\":\"White\",\"yards\":[91,192,293,194,95,196,297,198,199,100,201,302,203,104,205,306,207,208]}]}",
        fixture);
  assert(fclose(fixture) == 0);

  Course courses[16]{};
  assert(golfLoadSdCourses(courses, 16) == 1);
  SetupState setup{};
  initializeSetup(setup);
  setup.course = &courses[0];
  setup.teeIndex[0] = 1;
  GolfRound round{};
  assert(buildRoundFromSetup(setup, round) && RoundStore::write(round));
  GolfRound loaded{};
  assert(RoundStore::read(loaded).status == GolfJsonStatus::Ok);
  assert(strcmp(loaded.courseName, "SD Home") == 0 && loaded.par[0] == 3 && loaded.par[2] == 5);
  assert(strcmp(loaded.players[0].tee, "White") == 0 && loaded.players[0].yards[0] == 91 &&
         loaded.players[0].yards[17] == 208);

  assert(RoundStore::clear() && unlink(fixturePath) == 0 && rmdir(coursesPath) == 0 && rmdir(root) == 0);
  unsetenv("SCORECARD_DIR");
}

}  // namespace

int main() {
  testCourseFormatting();
  testPlayerCount();
  testBuildRound(1);
  testBuildRound(3);
  testKeyboard();
  testHomeSummary();
  testSyntheticSetupWalks();
  testSdCourseSetupWalk();
  return 0;
}
