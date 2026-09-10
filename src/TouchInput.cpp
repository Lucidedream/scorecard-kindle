#include "TouchInput.h"

#include <stdlib.h>

void GestureClassifier::begin(const uint64_t timeMs, const int x, const int y) {
  downTimeMs = timeMs;
  downX = x;
  downY = y;
  maximumTravelSquared = 0;
}

void GestureClassifier::update(const int x, const int y) {
  const int dx = x - downX;
  const int dy = y - downY;
  const int travelSquared = dx * dx + dy * dy;
  if (travelSquared > maximumTravelSquared) maximumTravelSquared = travelSquared;
}

TouchEvent GestureClassifier::finish(const uint64_t timeMs, const int x, const int y) {
  update(x, y);
  const int dx = x - downX;
  const int dy = y - downY;
  TouchEvent::Kind kind = TouchEvent::Kind::Tap;
  if (abs(dx) > SWIPE_DISTANCE && abs(dx) > 2 * abs(dy)) {
    kind = dx < 0 ? TouchEvent::Kind::SwipeLeft : TouchEvent::Kind::SwipeRight;
  } else if (timeMs - downTimeMs > LONG_PRESS_MS &&
             maximumTravelSquared < LONG_PRESS_TRAVEL * LONG_PRESS_TRAVEL) {
    kind = TouchEvent::Kind::LongPress;
  }
  return {kind, x, y};
}

#if defined(__linux__)

#include <fcntl.h>
#include <linux/input.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

namespace {

constexpr int SCREEN_WIDTH = 1072;
constexpr int SCREEN_HEIGHT = 1448;

bool bitIsSet(const unsigned long* bits, const int bit) {
  constexpr int BITS_PER_WORD = static_cast<int>(sizeof(unsigned long) * 8);
  return (bits[bit / BITS_PER_WORD] & (1UL << (bit % BITS_PER_WORD))) != 0;
}

int scaleAxis(const int value, const int minimum, const int maximum, const int screenExtent) {
  if (maximum <= minimum) return value;
  const long numerator = static_cast<long>(value - minimum) * (screenExtent - 1);
  const int scaled = static_cast<int>(numerator / (maximum - minimum));
  if (scaled < 0) return 0;
  if (scaled >= screenExtent) return screenExtent - 1;
  return scaled;
}

// The kernel stamps each input_event, but musl's time64 <linux/input.h> drops the
// `time` member, so read a monotonic clock instead. The few microseconds of skew
// versus the event stamp do not matter against a 450 ms long-press threshold.
uint64_t nowMs() {
  timespec now{};
  clock_gettime(CLOCK_MONOTONIC, &now);
  return static_cast<uint64_t>(now.tv_sec) * 1000U + static_cast<uint64_t>(now.tv_nsec) / 1000000U;
}

}  // namespace

TouchInput::~TouchInput() {
  if (fd >= 0) {
    ioctl(fd, EVIOCGRAB, 0);
    close(fd);
  }
}

bool TouchInput::openDevice() {
  constexpr int WORD_COUNT = (ABS_MAX + static_cast<int>(sizeof(unsigned long) * 8)) /
                             static_cast<int>(sizeof(unsigned long) * 8);
  unsigned long absBits[WORD_COUNT]{};
  unsigned long keyBits[(KEY_MAX + static_cast<int>(sizeof(unsigned long) * 8)) /
                        static_cast<int>(sizeof(unsigned long) * 8)]{};

  for (int index = 0; index < 32; ++index) {
    char path[64];
    snprintf(path, sizeof(path), "/dev/input/event%d", index);
    const int candidate = ::open(path, O_RDONLY | O_NONBLOCK);
    if (candidate < 0) continue;

    memset(absBits, 0, sizeof(absBits));
    if (ioctl(candidate, EVIOCGBIT(EV_ABS, sizeof(absBits)), absBits) < 0) {
      close(candidate);
      continue;
    }

    const bool multi = bitIsSet(absBits, ABS_MT_POSITION_X) && bitIsSet(absBits, ABS_MT_POSITION_Y);
    const bool single = bitIsSet(absBits, ABS_X) && bitIsSet(absBits, ABS_Y);
    memset(keyBits, 0, sizeof(keyBits));
    const bool touchKey = ioctl(candidate, EVIOCGBIT(EV_KEY, sizeof(keyBits)), keyBits) >= 0 &&
                          bitIsSet(keyBits, BTN_TOUCH);
    if (!multi && !(single && touchKey)) {
      close(candidate);
      continue;
    }

    fd = candidate;
    usesMultitouch = multi;
    ioctl(fd, EVIOCGNAME(sizeof(name)), name);
    hasTouchKey = touchKey;

    input_absinfo axis{};
    const int xCode = usesMultitouch ? ABS_MT_POSITION_X : ABS_X;
    const int yCode = usesMultitouch ? ABS_MT_POSITION_Y : ABS_Y;
    if (ioctl(fd, EVIOCGABS(xCode), &axis) >= 0) {
      minX = axis.minimum;
      maxX = axis.maximum;
    }
    if (ioctl(fd, EVIOCGABS(yCode), &axis) >= 0) {
      minY = axis.minimum;
      maxY = axis.maximum;
    }

    const int flags = fcntl(fd, F_GETFL, 0);
    if (flags >= 0) fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);
    ioctl(fd, EVIOCGRAB, 1);
    return true;
  }
  return false;
}

bool TouchInput::waitForEvent(TouchEvent& touchEvent) {
  if (fd < 0) return false;
  int rawX = minX;
  int rawY = minY;
  bool haveX = false;
  bool haveY = false;
  bool trackingTouch = false;
  bool downPending = false;
  uint64_t downTimeMs = 0;
  GestureClassifier classifier;

  while (true) {
    input_event event{};
    const ssize_t count = read(fd, &event, sizeof(event));
    if (count < 0 && errno == EINTR) continue;
    if (count != static_cast<ssize_t>(sizeof(event))) return false;

    if (event.type == EV_ABS) {
      if (event.code == (usesMultitouch ? ABS_MT_POSITION_X : ABS_X)) {
        rawX = event.value;
        haveX = true;
      } else if (event.code == (usesMultitouch ? ABS_MT_POSITION_Y : ABS_Y)) {
        rawY = event.value;
        haveY = true;
      } else if (usesMultitouch && event.code == ABS_MT_TRACKING_ID) {
        if (event.value >= 0) {
          trackingTouch = true;
          downPending = true;
          downTimeMs = nowMs();
          haveX = false;
          haveY = false;
        } else if (trackingTouch && haveX && haveY) {
          const int x = scaleAxis(rawX, minX, maxX, SCREEN_WIDTH);
          const int y = scaleAxis(rawY, minY, maxY, SCREEN_HEIGHT);
          if (downPending) classifier.begin(downTimeMs, x, y);
          touchEvent = classifier.finish(nowMs(), x, y);
          return true;
        }
      }
    } else if (event.type == EV_KEY && event.code == BTN_TOUCH) {
      if (event.value != 0) {
        trackingTouch = true;
        downPending = true;
        downTimeMs = nowMs();
      } else if (trackingTouch && haveX && haveY) {
        const int x = scaleAxis(rawX, minX, maxX, SCREEN_WIDTH);
        const int y = scaleAxis(rawY, minY, maxY, SCREEN_HEIGHT);
        if (downPending) classifier.begin(downTimeMs, x, y);
        touchEvent = classifier.finish(nowMs(), x, y);
        return true;
      }
    } else if (event.type == EV_SYN && event.code == SYN_REPORT && haveX && haveY) {
      const int x = scaleAxis(rawX, minX, maxX, SCREEN_WIDTH);
      const int y = scaleAxis(rawY, minY, maxY, SCREEN_HEIGHT);
      if (trackingTouch) {
        if (downPending) {
          classifier.begin(downTimeMs, x, y);
          downPending = false;
        } else {
          classifier.update(x, y);
        }
      } else if (!usesMultitouch && !hasTouchKey) {
        touchEvent = {TouchEvent::Kind::Tap, x, y};
        return true;
      }
    }
  }
}

#else

TouchInput::~TouchInput() = default;
bool TouchInput::openDevice() { return false; }
bool TouchInput::waitForEvent(TouchEvent&) { return false; }

#endif
