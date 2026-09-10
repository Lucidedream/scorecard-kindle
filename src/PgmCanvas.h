#pragma once

#include <stddef.h>
#include <stdint.h>

enum class TextSize { Small, Body, Display };
enum class TextAlign { Left, Center, Right };

class PgmCanvas {
 public:
  static constexpr int WIDTH = 1072;
  static constexpr int HEIGHT = 1448;

  void clear(bool black = false);
  void fillRect(int x, int y, int width, int height, bool black = true);
  void drawRect(int x, int y, int width, int height, int thickness = 4);
  void drawText(int x, int y, const char* utf8, TextSize size, TextAlign align = TextAlign::Left,
                bool inverted = false);
  int measureText(const char* utf8, TextSize size) const;
  int lineHeight(TextSize size) const;
  bool write(const char* path) const;

 private:
  void setPixel(int x, int y, bool black);
  void drawGlyph(int x, int y, uint32_t codepoint, TextSize size, bool black);

  uint8_t pixels[static_cast<size_t>(WIDTH * HEIGHT)]{};
};
