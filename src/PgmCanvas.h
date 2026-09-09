#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

class PgmCanvas {
 public:
  static constexpr int WIDTH = 1072;
  static constexpr int HEIGHT = 1448;

  void clear(bool black = false);
  void fillRect(int x, int y, int width, int height, bool black = true);
  void drawRect(int x, int y, int width, int height, int thickness = 4);
  void drawText(int x, int y, const char* text, int scale = 6, bool black = true);
  void drawTextCentered(int centerX, int y, const char* text, int scale = 6, bool black = true);
  bool write(const char* path) const;

 private:
  void setPixel(int x, int y, bool black);
  void drawChar(int x, int y, char ch, int scale, bool black);

  std::array<uint8_t, static_cast<std::size_t>(WIDTH * HEIGHT)> pixels{};
};
