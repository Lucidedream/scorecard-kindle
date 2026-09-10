#include "PgmCanvas.h"

#include "FontData.h"

#include <stdio.h>

namespace {

int capHeight(const TextSize size) {
  switch (size) {
    case TextSize::Small: return 22;
    case TextSize::Body: return 32;
    case TextSize::Display: return 62;
  }
  return 32;
}

int scaledEdge(const int sourcePixel, const TextSize size) {
  return (sourcePixel * capHeight(size) + 3) / 7;
}

const uint8_t* glyphFor(const uint32_t codepoint) {
  if (codepoint < 128) return font_data::BASIC[codepoint];
  if (codepoint >= 0xA0 && codepoint <= 0xFF) return font_data::LATIN1[codepoint - 0xA0];
  return font_data::BASIC[static_cast<unsigned int>('?')];
}

uint32_t nextCodepoint(const char*& cursor) {
  const auto first = static_cast<uint8_t>(*cursor++);
  if (first < 0x80) return first;
  int continuationCount = 0;
  uint32_t codepoint = 0;
  if (first >= 0xC2 && first <= 0xDF) {
    continuationCount = 1;
    codepoint = first & 0x1F;
  } else if (first >= 0xE0 && first <= 0xEF) {
    continuationCount = 2;
    codepoint = first & 0x0F;
  } else if (first >= 0xF0 && first <= 0xF4) {
    continuationCount = 3;
    codepoint = first & 0x07;
  }
  if (continuationCount == 0) return '?';
  for (int index = 0; index < continuationCount; ++index) {
    const auto next = static_cast<uint8_t>(cursor[index]);
    if ((next & 0xC0) != 0x80) return '?';
    codepoint = (codepoint << 6) | (next & 0x3F);
  }
  cursor += continuationCount;
  if (codepoint == 0x2013 || codepoint == 0x2014) return '-';
  if (codepoint == 0x2018 || codepoint == 0x2019) return '\'';
  if (codepoint == 0x201C || codepoint == 0x201D) return '"';
  return codepoint;
}

void horizontalBounds(const uint8_t* glyph, int& left, int& right) {
  left = 8;
  right = -1;
  for (int row = 0; row < 8; ++row) {
    for (int column = 0; column < 8; ++column) {
      if ((glyph[row] & (1U << column)) == 0) continue;
      if (column < left) left = column;
      if (column > right) right = column;
    }
  }
}

int glyphAdvance(const uint32_t codepoint, const TextSize size) {
  if (codepoint == ' ' || codepoint == 0xA0) return scaledEdge(4, size);
  int left;
  int right;
  horizontalBounds(glyphFor(codepoint), left, right);
  if (right < left) return scaledEdge(4, size);
  return scaledEdge(right - left + 1, size) + scaledEdge(1, size);
}

}  // namespace

void PgmCanvas::clear(const bool black) {
  for (size_t index = 0; index < sizeof(pixels); ++index) pixels[index] = black ? 0x00 : 0xFF;
}

void PgmCanvas::setPixel(const int x, const int y, const bool black) {
  if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT) return;
  pixels[static_cast<size_t>(y * WIDTH + x)] = black ? 0x00 : 0xFF;
}

void PgmCanvas::fillRect(const int x, const int y, const int width, const int height, const bool black) {
  const int left = x > 0 ? x : 0;
  const int top = y > 0 ? y : 0;
  const int right = x + width < WIDTH ? x + width : WIDTH;
  const int bottom = y + height < HEIGHT ? y + height : HEIGHT;
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

void PgmCanvas::drawGlyph(const int x, const int y, const uint32_t codepoint, const TextSize size,
                          const bool black) {
  const uint8_t* glyph = glyphFor(codepoint);
  int left;
  int right;
  horizontalBounds(glyph, left, right);
  if (right < left) return;

  for (int row = 0; row < 8; ++row) {
    for (int column = left; column <= right; ++column) {
      if ((glyph[row] & (1U << column)) == 0) continue;
      const int pixelLeft = scaledEdge(column - left, size);
      const int pixelRight = scaledEdge(column - left + 1, size);
      const int pixelTop = scaledEdge(row, size);
      const int pixelBottom = scaledEdge(row + 1, size);
      fillRect(x + pixelLeft, y + pixelTop, pixelRight - pixelLeft, pixelBottom - pixelTop, black);
    }
  }
}

int PgmCanvas::measureText(const char* utf8, const TextSize size) const {
  if (utf8 == nullptr) return 0;
  int width = 0;
  for (const char* cursor = utf8; *cursor != '\0';) width += glyphAdvance(nextCodepoint(cursor), size);
  return width == 0 ? 0 : width - scaledEdge(1, size);
}

int PgmCanvas::lineHeight(const TextSize size) const { return scaledEdge(9, size); }

void PgmCanvas::drawText(int x, const int y, const char* utf8, const TextSize size, const TextAlign align,
                         const bool inverted) {
  if (utf8 == nullptr) return;
  const int width = measureText(utf8, size);
  if (align == TextAlign::Center) x -= width / 2;
  if (align == TextAlign::Right) x -= width;

  for (const char* cursor = utf8; *cursor != '\0';) {
    const uint32_t codepoint = nextCodepoint(cursor);
    drawGlyph(x, y, codepoint, size, !inverted);
    x += glyphAdvance(codepoint, size);
  }
}

bool PgmCanvas::write(const char* path) const {
  FILE* file = fopen(path, "wb");
  if (file == nullptr) return false;
  const int headerResult = fprintf(file, "P5\n%d %d\n255\n", WIDTH, HEIGHT);
  const size_t written = fwrite(pixels, 1, sizeof(pixels), file);
  const int closeResult = fclose(file);
  return headerResult > 0 && written == sizeof(pixels) && closeResult == 0;
}
