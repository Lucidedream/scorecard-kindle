#include "PgmCanvas.h"

#include "FontData.h"

#include <stdio.h>

namespace {

int sizeIndex(const TextSize size) { return static_cast<int>(size); }

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

const font_data::Glyph& glyphFor(uint32_t codepoint, const TextSize size) {
  if (codepoint < font_data::FIRST_CODEPOINT ||
      codepoint >= font_data::FIRST_CODEPOINT + font_data::GLYPH_COUNT) {
    codepoint = '?';
  }
  return font_data::GLYPHS[sizeIndex(size)][codepoint - font_data::FIRST_CODEPOINT];
}

}  // namespace

void PgmCanvas::clear(const bool black) {
  for (size_t index = 0; index < sizeof(pixels); ++index) pixels[index] = black ? 0x00 : 0xFF;
}

void PgmCanvas::setPixel(const int x, const int y, const bool black) {
  if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT) return;
  pixels[static_cast<size_t>(y * WIDTH + x)] = black ? 0x00 : 0xFF;
}

void PgmCanvas::blendPixel(const int x, const int y, const uint8_t coverage, const uint8_t shade) {
  if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT || coverage == 0) return;
  const size_t index = static_cast<size_t>(y * WIDTH + x);
  const int background = pixels[index];
  const int foreground = shade;
  pixels[index] = static_cast<uint8_t>((background * (255 - coverage) + foreground * coverage + 127) / 255);
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
                          const uint8_t shade) {
  const font_data::Glyph& glyph = glyphFor(codepoint, size);
  const font_data::Metrics& metrics = font_data::METRICS[sizeIndex(size)];
  const int left = x + glyph.leftBearing;
  const int top = y + metrics.capHeight - glyph.topBearing;
  const int pixelCount = glyph.width * glyph.height;
  for (int pixel = 0; pixel < pixelCount; ++pixel) {
    const uint8_t packed = font_data::BITMAP[glyph.bitmapOffset + static_cast<unsigned int>(pixel / 2)];
    const uint8_t coverage = static_cast<uint8_t>((pixel % 2 == 0 ? packed >> 4 : packed & 0x0F) * 17);
    blendPixel(left + pixel % glyph.width, top + pixel / glyph.width, coverage, shade);
  }
}

int PgmCanvas::measureText(const char* utf8, const TextSize size) const {
  if (utf8 == nullptr) return 0;
  int width = 0;
  for (const char* cursor = utf8; *cursor != '\0';) width += glyphFor(nextCodepoint(cursor), size).xAdvance;
  return width;
}

int PgmCanvas::lineHeight(const TextSize size) const {
  const font_data::Metrics& metrics = font_data::METRICS[sizeIndex(size)];
  return metrics.ascent - metrics.descent + metrics.lineGap;
}

void PgmCanvas::drawText(int x, const int y, const char* utf8, const TextSize size, const TextAlign align,
                         const bool inverted, const uint8_t shade) {
  if (utf8 == nullptr) return;
  const int width = measureText(utf8, size);
  if (align == TextAlign::Center) x -= width / 2;
  if (align == TextAlign::Right) x -= width;

  for (const char* cursor = utf8; *cursor != '\0';) {
    const uint32_t codepoint = nextCodepoint(cursor);
    const font_data::Glyph& glyph = glyphFor(codepoint, size);
    drawGlyph(x, y, codepoint, size, inverted ? 255 : shade);
    x += glyph.xAdvance;
  }
}

void PgmCanvas::drawMonoText(int x, const int y, const char* utf8, const TextSize size,
                             const TextAlign align, const bool inverted, const uint8_t shade) {
  if (utf8 == nullptr) return;
  int count = 0;
  for (const char* cursor = utf8; *cursor != '\0';) {
    nextCodepoint(cursor);
    ++count;
  }
  const int advance = glyphFor('0', size).xAdvance + 2;
  const int width = count * advance;
  if (align == TextAlign::Center) x -= width / 2;
  if (align == TextAlign::Right) x -= width;
  for (const char* cursor = utf8; *cursor != '\0';) {
    const uint32_t codepoint = nextCodepoint(cursor);
    const font_data::Glyph& glyph = glyphFor(codepoint, size);
    drawGlyph(x + (advance - glyph.xAdvance) / 2, y, codepoint, size, inverted ? 255 : shade);
    x += advance;
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
