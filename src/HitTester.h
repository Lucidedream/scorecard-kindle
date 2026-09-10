#pragma once

struct Rect {
  int x;
  int y;
  int width;
  int height;

  bool contains(const int pointX, const int pointY) const {
    return width > 0 && height > 0 && pointX >= x && pointX < x + width && pointY >= y && pointY < y + height;
  }
};

// UI code should make interactive regions at least 130 px on each axis.
class HitTester {
 public:
  void add(const Rect rect, const int actionId) {
    if (count < MAX_REGIONS) regions[count++] = {rect, actionId};
  }
  void clear() { count = 0; }

  int hitTest(const int x, const int y) const {
    for (int index = 0; index < count; ++index) {
      if (regions[index].rect.contains(x, y)) return regions[index].actionId;
    }
    return -1;
  }

 private:
  static constexpr int MAX_REGIONS = 64;
  struct Region {
    Rect rect;
    int actionId;
  };

  Region regions[MAX_REGIONS]{};
  int count = 0;
};
