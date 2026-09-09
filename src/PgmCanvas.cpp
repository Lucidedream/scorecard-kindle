#include "PgmCanvas.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace {

struct Glyph {
  uint8_t rows[7];
};

constexpr Glyph glyphFor(char ch) {
  switch (ch) {
    case 'A': return {{14, 17, 17, 31, 17, 17, 17}};
    case 'B': return {{30, 17, 17, 30, 17, 17, 30}};
    case 'C': return {{14, 17, 16, 16, 16, 17, 14}};
    case 'D': return {{30, 17, 17, 17, 17, 17, 30}};
    case 'E': return {{31, 16, 16, 30, 16, 16, 31}};
    case 'F': return {{31, 16, 16, 30, 16, 16, 16}};
    case 'G': return {{14, 17, 16, 23, 17, 17, 14}};
    case 'H': return {{17, 17, 17, 31, 17, 17, 17}};
    case 'I': return {{31, 4, 4, 4, 4, 4, 31}};
    case 'J': return {{1, 1, 1, 1, 17, 17, 14}};
    case 'K': return {{17, 18, 20, 24, 20, 18, 17}};
    case 'L': return {{16, 16, 16, 16, 16, 16, 31}};
    case 'M': return {{17, 27, 21, 21, 17, 17, 17}};
    case 'N': return {{17, 25, 21, 19, 17, 17, 17}};
    case 'O': return {{14, 17, 17, 17, 17, 17, 14}};
    case 'P': return {{30, 17, 17, 30, 16, 16, 16}};
    case 'Q': return {{14, 17, 17, 17, 21, 18, 13}};
    case 'R': return {{30, 17, 17, 30, 20, 18, 17}};
    case 'S': return {{15, 16, 16, 14, 1, 1, 30}};
    case 'T': return {{31, 4, 4, 4, 4, 4, 4}};
    case 'U': return {{17, 17, 17, 17, 17, 17, 14}};
    case 'V': return {{17, 17, 17, 17, 17, 10, 4}};
    case 'W': return {{17, 17, 17, 21, 21, 21, 10}};
    case 'X': return {{17, 17, 10, 4, 10, 17, 17}};
    case 'Y': return {{17, 17, 10, 4, 4, 4, 4}};
    case 'Z': return {{31, 1, 2, 4, 8, 16, 31}};
    case '0': return {{14, 17, 19, 21, 25, 17, 14}};
    case '1': return {{4, 12, 4, 4, 4, 4, 14}};
    case '2': return {{14, 17, 1, 2, 4, 8, 31}};
    case '3': return {{30, 1, 1, 14, 1, 1, 30}};
    case '4': return {{2, 6, 10, 18, 31, 2, 2}};
    case '5': return {{31, 16, 16, 30, 1, 1, 30}};
    case '6': return {{14, 16, 16, 30, 17, 17, 14}};
    case '7': return {{31, 1, 2, 4, 8, 8, 8}};
    case '8': return {{14, 17, 17, 14, 17, 17, 14}};
    case '9': return {{14, 17, 17, 15, 1, 1, 14}};
    case '+': return {{0, 4, 4, 31, 4, 4, 0}};
    case '-': return {{0, 0, 0, 31, 0, 0, 0}};
    case '/': return {{1, 1, 2, 4, 8, 16, 16}};
    case ':': return {{0, 4, 4, 0, 4, 4, 0}};
    case '.': return {{0, 0, 0, 0, 0, 12, 12}};
    default: return {{0, 0, 0, 0, 0, 0, 0}};
  }
}

}  // namespace

void PgmCanvas::clear(const bool black) { pixels.fill(black ? 0x00 : 0xFF); }

void PgmCanvas::setPixel(const int x, const int y, const bool black) {
  if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT) return;
  const std::size_t index = static_cast<std::size_t>(y * WIDTH + x);
  pixels[index] = black ? 0x00 : 0xFF;
}

void PgmCanvas::fillRect(int x, int y, int width, int height, const bool black) {
  const int left = std::max(0, x);
  const int top = std::max(0, y);
  const int right = std::min(WIDTH, x + width);
  const int bottom = std::min(HEIGHT, y + height);
  for (int py = top; py < bottom; ++py) {
    for (int px = left; px < right; ++px) setPixel(px, py, black);
  }
}

void PgmCanvas::drawRect(const int x, const int y, const int width, const int height, const int thickness) {
  fillRect(x, y, width, thickness);
  fillRect(x, y + height - thickness, width, thickness);
  fillRect(x, y, thickness, height);
  fillRect(x + width - thickness, y, thickness, height);
}

void PgmCanvas::drawChar(const int x, const int y, const char ch, const int scale, const bool black) {
  const Glyph glyph = glyphFor(ch);
  for (int row = 0; row < 7; ++row) {
    for (int col = 0; col < 5; ++col) {
      if ((glyph.rows[row] & (1U << (4 - col))) != 0) {
        fillRect(x + col * scale, y + row * scale, scale, scale, black);
      }
    }
  }
}

void PgmCanvas::drawText(int x, const int y, const char* text, const int scale, const bool black) {
  if (text == nullptr) return;
  for (const char* cursor = text; *cursor != '\0'; ++cursor) {
    drawChar(x, y, *cursor, scale, black);
    x += 6 * scale;
  }
}

void PgmCanvas::drawTextCentered(const int centerX, const int y, const char* text, const int scale,
                                 const bool black) {
  const int width = static_cast<int>(std::strlen(text)) * 6 * scale - scale;
  drawText(centerX - width / 2, y, text, scale, black);
}

bool PgmCanvas::write(const char* path) const {
  FILE* file = std::fopen(path, "wb");
  if (file == nullptr) return false;
  const int headerResult = std::fprintf(file, "P5\n%d %d\n255\n", WIDTH, HEIGHT);
  const std::size_t written = std::fwrite(pixels.data(), 1, pixels.size(), file);
  const int closeResult = std::fclose(file);
  return headerResult > 0 && written == pixels.size() && closeResult == 0;
}
