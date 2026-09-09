#pragma once

struct TouchPoint {
  int x;
  int y;
};

class TouchInput {
 public:
  TouchInput() = default;
  ~TouchInput();

  TouchInput(const TouchInput&) = delete;
  TouchInput& operator=(const TouchInput&) = delete;

  bool openDevice();
  bool waitForTap(TouchPoint& point);
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
