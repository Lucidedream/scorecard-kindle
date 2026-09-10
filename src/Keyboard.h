#pragma once

#include <stddef.h>

#include "HitTester.h"
#include "PgmCanvas.h"
#include "TouchInput.h"

enum KeyboardAction {
  KeyboardNone = 300,
  KeyboardLetterFirst,
  KeyboardShift = KeyboardLetterFirst + 26,
  KeyboardBackspace,
  KeyboardSpace,
  KeyboardDone,
};

struct KeyboardState {
  char original[24];
  char text[24];
  bool shift;
  bool finished;
  bool committed;
};

void initializeKeyboard(KeyboardState& state, const char* initial);
void drawKeyboard(PgmCanvas& canvas, HitTester& hits, const KeyboardState& state);
bool handleKeyboardAction(KeyboardState& state, int action, TouchEvent::Kind kind);
bool finishKeyboard(const KeyboardState& state, char* output, size_t capacity);
