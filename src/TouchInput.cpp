#include "TouchInput.h"

#if defined(__linux__)

#include <fcntl.h>
#include <linux/input.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <cstring>

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
    std::snprintf(path, sizeof(path), "/dev/input/event%d", index);
    const int candidate = ::open(path, O_RDONLY | O_NONBLOCK);
    if (candidate < 0) continue;

    std::memset(absBits, 0, sizeof(absBits));
    if (ioctl(candidate, EVIOCGBIT(EV_ABS, sizeof(absBits)), absBits) < 0) {
      close(candidate);
      continue;
    }

    const bool multi = bitIsSet(absBits, ABS_MT_POSITION_X) && bitIsSet(absBits, ABS_MT_POSITION_Y);
    const bool single = bitIsSet(absBits, ABS_X) && bitIsSet(absBits, ABS_Y);
    std::memset(keyBits, 0, sizeof(keyBits));
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

bool TouchInput::waitForTap(TouchPoint& point) {
  if (fd < 0) return false;
  int rawX = minX;
  int rawY = minY;
  bool haveX = false;
  bool haveY = false;
  bool trackingTouch = false;

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
        } else if (trackingTouch && haveX && haveY) {
          point.x = scaleAxis(rawX, minX, maxX, SCREEN_WIDTH);
          point.y = scaleAxis(rawY, minY, maxY, SCREEN_HEIGHT);
          return true;
        }
      }
    } else if (event.type == EV_KEY && event.code == BTN_TOUCH) {
      if (event.value != 0) {
        trackingTouch = true;
      } else if (trackingTouch && haveX && haveY) {
        point.x = scaleAxis(rawX, minX, maxX, SCREEN_WIDTH);
        point.y = scaleAxis(rawY, minY, maxY, SCREEN_HEIGHT);
        return true;
      }
    } else if (event.type == EV_SYN && event.code == SYN_REPORT && !usesMultitouch && !hasTouchKey && haveX &&
               haveY) {
      point.x = scaleAxis(rawX, minX, maxX, SCREEN_WIDTH);
      point.y = scaleAxis(rawY, minY, maxY, SCREEN_HEIGHT);
      return true;
    }
  }
}

#else

TouchInput::~TouchInput() = default;
bool TouchInput::openDevice() { return false; }
bool TouchInput::waitForTap(TouchPoint&) { return false; }

#endif
