#include "HitTester.h"
#include "PgmCanvas.h"
#include "TouchInput.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

namespace {

constexpr char SCREEN_PATH[] = "/tmp/scorecard-screen.pgm";
constexpr char DIAGNOSTIC_SCREEN_PATH[] = "/mnt/us/scorecard/screen.pgm";
constexpr char RUNTIME_LOG_PATH[] = "/mnt/us/scorecard/runtime.log";

enum Action { Exit = 1, Previous, Hold, Next };

PgmCanvas canvas;
HitTester hitTester;

const char* kindName(const TouchEvent::Kind kind) {
  switch (kind) {
    case TouchEvent::Kind::Tap: return "Tap";
    case TouchEvent::Kind::SwipeLeft: return "SwipeLeft";
    case TouchEvent::Kind::SwipeRight: return "SwipeRight";
    case TouchEvent::Kind::LongPress: return "LongPress";
  }
  return "Unknown";
}

void drawButton(const Rect rect, const char* label) {
  canvas.drawRect(rect.x, rect.y, rect.width, rect.height, 5);
  canvas.drawText(rect.x + rect.width / 2, rect.y + (rect.height - canvas.lineHeight(TextSize::Body)) / 2,
                  label, TextSize::Body, TextAlign::Center);
}

bool render(const char* path, const bool showOnDevice, const bool fullRefresh) {
  hitTester.clear();
  canvas.clear();

  canvas.fillRect(0, 0, PgmCanvas::WIDTH, 130);
  canvas.drawText(42, 30, "Scorecard", TextSize::Display, TextAlign::Left, true);
  canvas.drawText(PgmCanvas::WIDTH - 38, 47, "Exit", TextSize::Body, TextAlign::Right, true);

  canvas.drawText(PgmCanvas::WIDTH / 2, 185, "Mixed-case bitmap typography", TextSize::Body,
                  TextAlign::Center);
  canvas.drawText(54, 275, "Café Royal & São Miguel — championship tees", TextSize::Small);
  canvas.drawText(54, 325, "The quick brown fox jumps over 18 lazy dogs.", TextSize::Small);
  canvas.drawText(PgmCanvas::WIDTH - 54, 395, "Right aligned: par 4, +2", TextSize::Body,
                  TextAlign::Right);

  const Rect previous{45, 560, 290, 180};
  const Rect hold{391, 560, 290, 180};
  const Rect next{737, 560, 290, 180};
  const Rect exit{890, 0, 182, 130};
  drawButton(previous, "Previous");
  drawButton(hold, "Hold me");
  drawButton(next, "Next");
  hitTester.add(exit, Action::Exit);
  hitTester.add(previous, Action::Previous);
  hitTester.add(hold, Action::Hold);
  hitTester.add(next, Action::Next);

  canvas.drawText(PgmCanvas::WIDTH / 2, 825, "Tap a button", TextSize::Display, TextAlign::Center);
  canvas.drawText(PgmCanvas::WIDTH / 2, 950, "Swipe left or right anywhere", TextSize::Body,
                  TextAlign::Center);
  canvas.drawText(PgmCanvas::WIDTH / 2, 1010, "Long-press the center button", TextSize::Body,
                  TextAlign::Center);
  canvas.fillRect(45, 1115, 982, 3);
  canvas.drawText(54, 1160, "Small 22 px  /  Body 32 px  /  Display 62 px", TextSize::Small);
  canvas.drawText(54, 1215, "Latin-1: Ångström, jalapeño, déjà vu, £12.50", TextSize::Small);

  if (!canvas.write(path)) return false;
  if (!showOnDevice) return true;
  canvas.write(DIAGNOSTIC_SCREEN_PATH);
  const char* command = fullRefresh
                            ? "/mnt/us/libkh/bin/fbink -c -f -W GC16 -i /tmp/scorecard-screen.pgm "
                              ">/mnt/us/scorecard/fbink.log 2>&1"
                            : "/mnt/us/libkh/bin/fbink -c -W DU -i /tmp/scorecard-screen.pgm "
                              ">/mnt/us/scorecard/fbink.log 2>&1";
  return system(command) == 0;
}

void logEvent(const TouchEvent& touchEvent, const int action) {
  FILE* log = fopen(RUNTIME_LOG_PATH, "a");
  if (log == nullptr) return;
  fprintf(log, "TouchEvent kind=%s x=%d y=%d action=%d\n", kindName(touchEvent.kind), touchEvent.x,
          touchEvent.y, action);
  fclose(log);
}

}  // namespace

int main(const int argc, char** argv) {
  if (argc == 3 && strcmp(argv[1], "--render") == 0) return render(argv[2], false, true) ? 0 : 1;

  TouchInput input;
  if (!input.openDevice()) {
    fprintf(stderr, "Could not find the Kindle touchscreen\n");
    return 2;
  }

  FILE* log = fopen(RUNTIME_LOG_PATH, "a");
  if (log != nullptr) {
    fprintf(log, "Started with touchscreen: %s\n", input.deviceName());
    fclose(log);
  }

  bool running = true;
  unsigned int paints = 0;
  while (running) {
    if (!render(SCREEN_PATH, true, paints == 0 || paints % 8 == 0)) return 3;
    ++paints;
    TouchEvent touchEvent{};
    if (!input.waitForEvent(touchEvent)) return 4;
    const int action = hitTester.hitTest(touchEvent.x, touchEvent.y);
    logEvent(touchEvent, action);
    if (touchEvent.kind == TouchEvent::Kind::Tap && action == Action::Exit) running = false;
  }

  unlink(SCREEN_PATH);
  return 0;
}
