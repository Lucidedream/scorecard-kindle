#pragma once

#include <stdint.h>

struct TouchEvent {
  enum class Kind { Tap, SwipeLeft, SwipeRight, LongPress };

  Kind kind;
  int x;
  int y;
  uint32_t durationMs;
};

enum class TouchWaitResult { Event, Timeout, Error };

class GestureClassifier {
 public:
  static constexpr int SWIPE_DISTANCE = 140;
  static constexpr int LONG_PRESS_MS = 450;
  static constexpr int LONG_PRESS_TRAVEL = 40;

  void begin(uint64_t timeMs, int x, int y);
  void update(int x, int y);
  TouchEvent finish(uint64_t timeMs, int x, int y);

 private:
  uint64_t downTimeMs = 0;
  int downX = 0;
  int downY = 0;
  int maximumTravelSquared = 0;
};

class TouchInput {
 public:
  TouchInput() = default;
  ~TouchInput();

  TouchInput(const TouchInput&) = delete;
  TouchInput& operator=(const TouchInput&) = delete;

  bool openDevice();
  bool waitForEvent(TouchEvent& touchEvent);
  TouchWaitResult waitForEvent(TouchEvent& touchEvent, int timeoutMs);
  const char* deviceName() const { return name; }

 private:
#if defined(__linux__)
  int fd = -1;
  int minX = 0;
  int maxX = 1079;
  int minY = 0;
  int maxY = 1439;
  bool usesMultitouch = false;
  bool hasTouchKey = false;
#endif
  char name[128]{};
};
