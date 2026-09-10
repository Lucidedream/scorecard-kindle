#include "Keyboard.h"

#include <stdio.h>
#include <string.h>

namespace {

constexpr char LETTERS[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";

void centered(PgmCanvas& canvas, const Rect rect, const char* label, const TextSize size,
              const bool inverted = false) {
  canvas.drawText(rect.x + rect.width / 2, rect.y + (rect.height - canvas.lineHeight(size)) / 2,
                  label, size, TextAlign::Center, inverted);
}

void key(PgmCanvas& canvas, HitTester& hits, const Rect rect, const char* label, const int action,
         const bool selected = false) {
  if (selected) canvas.fillRect(rect.x, rect.y, rect.width, rect.height);
  else canvas.drawRect(rect.x, rect.y, rect.width, rect.height, 2);
  centered(canvas, rect, label, TextSize::Body, selected);
  hits.add(rect, action);
}

void letterRow(PgmCanvas& canvas, HitTester& hits, const KeyboardState& state, const char* letters,
               const int count, const int y, const int left, const int width) {
  for (int index = 0; index < count; ++index) {
    const Rect rect{left + index * width, y, width - 6, 206};
    char label[2] = {letters[index], '\0'};
    if (!state.shift) label[0] = static_cast<char>(label[0] + ('a' - 'A'));
    key(canvas, hits, rect, label, KeyboardLetterFirst + letters[index] - 'A');
  }
}

}  // namespace

void initializeKeyboard(KeyboardState& state, const char* initial) {
  state = {};
  snprintf(state.original, sizeof(state.original), "%s", initial == nullptr ? "" : initial);
  snprintf(state.text, sizeof(state.text), "%s", state.original);
}

void drawKeyboard(PgmCanvas& canvas, HitTester& hits, const KeyboardState& state) {
  canvas.clear();
  hits.clear();
  canvas.drawText(46, 38, "EDIT NAME", TextSize::Small);
  canvas.drawRect(36, 96, 1000, 218, 3);
  canvas.drawText(62, 178, state.text, TextSize::Body);
  const int caretX = 66 + canvas.measureText(state.text, TextSize::Body);
  canvas.fillRect(caretX, 174, 4, canvas.lineHeight(TextSize::Body));

  letterRow(canvas, hits, state, "QWERTYUIOP", 10, 350, 16, 105);
  letterRow(canvas, hits, state, "ASDFGHJKL", 9, 574, 68, 105);
  key(canvas, hits, {16, 798, 150, 206}, "SHIFT", KeyboardShift, state.shift);
  letterRow(canvas, hits, state, "ZXCVBNM", 7, 798, 172, 105);
  key(canvas, hits, {913, 798, 143, 206}, "DEL", KeyboardBackspace);
  key(canvas, hits, {16, 1022, 690, 410}, "SPACE", KeyboardSpace);
  key(canvas, hits, {722, 1022, 334, 410}, "DONE", KeyboardDone, true);
}

bool handleKeyboardAction(KeyboardState& state, const int action, const TouchEvent::Kind kind) {
  if (action >= KeyboardLetterFirst && action < KeyboardLetterFirst + 26 && kind == TouchEvent::Kind::Tap) {
    const size_t length = strlen(state.text);
    if (length + 1 < sizeof(state.text)) {
      char value = LETTERS[action - KeyboardLetterFirst];
      if (!state.shift) value = static_cast<char>(value + ('a' - 'A'));
      state.text[length] = value;
      state.text[length + 1] = '\0';
    }
    state.shift = false;
    return true;
  }
  if (action == KeyboardShift && kind == TouchEvent::Kind::Tap) {
    state.shift = !state.shift;
    return true;
  }
  if (action == KeyboardBackspace && (kind == TouchEvent::Kind::Tap || kind == TouchEvent::Kind::LongPress)) {
    if (kind == TouchEvent::Kind::LongPress) state.text[0] = '\0';
    else {
      const size_t length = strlen(state.text);
      if (length != 0) state.text[length - 1] = '\0';
    }
    return true;
  }
  if (action == KeyboardSpace && kind == TouchEvent::Kind::Tap) {
    const size_t length = strlen(state.text);
    if (length != 0 && length + 1 < sizeof(state.text)) {
      state.text[length] = ' ';
      state.text[length + 1] = '\0';
    }
    return true;
  }
  if (action == KeyboardDone && kind == TouchEvent::Kind::Tap) {
    state.finished = true;
    state.committed = state.text[0] != '\0';
    return true;
  }
  return false;
}

bool finishKeyboard(const KeyboardState& state, char* output, const size_t capacity) {
  if (!state.finished || capacity == 0) return false;
  snprintf(output, capacity, "%s", state.committed ? state.text : state.original);
  return state.committed;
}
