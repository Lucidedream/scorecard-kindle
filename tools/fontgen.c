#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

enum { FIRST_CODEPOINT = 0x20, LAST_CODEPOINT = 0xFF, GLYPH_COUNT = 224, SIZE_COUNT = 3 };

typedef struct {
  uint32_t offset;
  uint16_t width;
  uint16_t height;
  int16_t advance;
  int16_t left;
  int16_t top;
} Glyph;

typedef struct {
  uint8_t* data;
  size_t size;
  size_t capacity;
} Buffer;

static const int PIXEL_HEIGHTS[SIZE_COUNT] = {35, 51, 100};

static int round_int(const float value) { return (int)(value >= 0.0f ? value + 0.5f : value - 0.5f); }

static void append(Buffer* buffer, const uint8_t value) {
  if (buffer->size == buffer->capacity) {
    const size_t capacity = buffer->capacity == 0 ? 65536 : buffer->capacity * 2;
    uint8_t* data = (uint8_t*)realloc(buffer->data, capacity);
    if (data == NULL) {
      fprintf(stderr, "fontgen: out of memory\n");
      exit(1);
    }
    buffer->data = data;
    buffer->capacity = capacity;
  }
  buffer->data[buffer->size++] = value;
}

static uint8_t* read_file(const char* path, long* size) {
  FILE* file = fopen(path, "rb");
  if (file == NULL || fseek(file, 0, SEEK_END) != 0) return NULL;
  *size = ftell(file);
  if (*size <= 0 || fseek(file, 0, SEEK_SET) != 0) return NULL;
  uint8_t* data = (uint8_t*)malloc((size_t)*size);
  if (data == NULL || fread(data, 1, (size_t)*size, file) != (size_t)*size || fclose(file) != 0) {
    free(data);
    return NULL;
  }
  return data;
}

static void write_glyphs(FILE* output, const Glyph glyphs[SIZE_COUNT][GLYPH_COUNT]) {
  fprintf(output, "inline constexpr Glyph GLYPHS[SIZE_COUNT][GLYPH_COUNT] = {\n");
  for (int size = 0; size < SIZE_COUNT; ++size) {
    fprintf(output, "  {\n");
    for (int index = 0; index < GLYPH_COUNT; ++index) {
      const Glyph* glyph = &glyphs[size][index];
      fprintf(output, "    {%u, %u, %u, %d, %d, %d},\n", glyph->offset, glyph->width, glyph->height,
              glyph->advance, glyph->left, glyph->top);
    }
    fprintf(output, "  },\n");
  }
  fprintf(output, "};\n\n");
}

static void write_bitmap(FILE* output, const Buffer* bitmap) {
  fprintf(output, "inline constexpr uint8_t BITMAP[%zu] = {\n", bitmap->size);
  for (size_t index = 0; index < bitmap->size; ++index) {
    if (index % 16 == 0) fprintf(output, "  ");
    fprintf(output, "0x%02x,%s", bitmap->data[index], index % 16 == 15 ? "\n" : " ");
  }
  if (bitmap->size % 16 != 0) fprintf(output, "\n");
  fprintf(output, "};\n\n");
}

int main(const int argc, char** argv) {
  if (argc != 3) {
    fprintf(stderr, "usage: fontgen font.ttf FontData.h\n");
    return 2;
  }

  long fontSize = 0;
  uint8_t* fontData = read_file(argv[1], &fontSize);
  if (fontData == NULL) {
    fprintf(stderr, "fontgen: could not read %s\n", argv[1]);
    return 1;
  }

  stbtt_fontinfo font;
  if (!stbtt_InitFont(&font, fontData, stbtt_GetFontOffsetForIndex(fontData, 0))) {
    fprintf(stderr, "fontgen: invalid font\n");
    free(fontData);
    return 1;
  }

  Glyph glyphs[SIZE_COUNT][GLYPH_COUNT] = {{{0}}};
  int metrics[SIZE_COUNT][4] = {{0}};
  Buffer bitmap = {0};
  int fontAscent;
  int fontDescent;
  int fontLineGap;
  stbtt_GetFontVMetrics(&font, &fontAscent, &fontDescent, &fontLineGap);

  for (int size = 0; size < SIZE_COUNT; ++size) {
    const float scale = stbtt_ScaleForPixelHeight(&font, (float)PIXEL_HEIGHTS[size]);
    int capX0;
    int capY0;
    int capX1;
    int capY1;
    stbtt_GetCodepointBitmapBox(&font, 'H', scale, scale, &capX0, &capY0, &capX1, &capY1);
    metrics[size][0] = round_int(fontAscent * scale);
    metrics[size][1] = round_int(fontDescent * scale);
    metrics[size][2] = round_int(fontLineGap * scale);
    metrics[size][3] = capY1 - capY0;

    for (int codepoint = FIRST_CODEPOINT; codepoint <= LAST_CODEPOINT; ++codepoint) {
      const int index = codepoint - FIRST_CODEPOINT;
      int advance;
      int leftBearing;
      int x0;
      int y0;
      int x1;
      int y1;
      stbtt_GetCodepointHMetrics(&font, codepoint, &advance, &leftBearing);
      stbtt_GetCodepointBitmapBox(&font, codepoint, scale, scale, &x0, &y0, &x1, &y1);
      const int width = x1 - x0;
      const int height = y1 - y0;
      Glyph* glyph = &glyphs[size][index];
      glyph->offset = (uint32_t)bitmap.size;
      glyph->width = (uint16_t)width;
      glyph->height = (uint16_t)height;
      glyph->advance = (int16_t)round_int(advance * scale);
      glyph->left = (int16_t)x0;
      glyph->top = (int16_t)-y0;

      if (width == 0 || height == 0) continue;
      uint8_t* coverage = stbtt_GetCodepointBitmap(&font, scale, scale, codepoint, &x1, &y1, NULL, NULL);
      if (coverage == NULL) {
        fprintf(stderr, "fontgen: could not rasterize U+%04X\n", codepoint);
        free(bitmap.data);
        free(fontData);
        return 1;
      }
      const int pixelCount = width * height;
      for (int pixel = 0; pixel < pixelCount; pixel += 2) {
        const uint8_t high = (uint8_t)((coverage[pixel] + 8) / 17);
        const uint8_t low = pixel + 1 < pixelCount ? (uint8_t)((coverage[pixel + 1] + 8) / 17) : 0;
        append(&bitmap, (uint8_t)((high << 4) | low));
      }
      stbtt_FreeBitmap(coverage, NULL);
    }
    fprintf(stderr, "fontgen: %d px font -> %d px cap height\n", PIXEL_HEIGHTS[size], metrics[size][3]);
  }

  FILE* output = fopen(argv[2], "wb");
  if (output == NULL) {
    fprintf(stderr, "fontgen: could not write %s\n", argv[2]);
    free(bitmap.data);
    free(fontData);
    return 1;
  }
  fprintf(output,
          "// Generated by tools/generate_font.sh. Do not edit.\n"
          "// Public Sans Medium v2.001, SIL Open Font License 1.1.\n"
          "#pragma once\n\n#include <stdint.h>\n\nnamespace font_data {\n\n"
          "inline constexpr int FIRST_CODEPOINT = 0x20;\n"
          "inline constexpr int GLYPH_COUNT = 224;\n"
          "inline constexpr int SIZE_COUNT = 3;\n\n"
          "struct Glyph {\n"
          "  uint32_t bitmapOffset;\n  uint16_t width;\n  uint16_t height;\n"
          "  int16_t xAdvance;\n  int16_t leftBearing;\n  int16_t topBearing;\n};\n\n"
          "struct Metrics {\n"
          "  int16_t ascent;\n  int16_t descent;\n  int16_t lineGap;\n  int16_t capHeight;\n};\n\n");
  fprintf(output, "inline constexpr Metrics METRICS[SIZE_COUNT] = {\n");
  for (int size = 0; size < SIZE_COUNT; ++size) {
    fprintf(output, "  {%d, %d, %d, %d},\n", metrics[size][0], metrics[size][1], metrics[size][2],
            metrics[size][3]);
  }
  fprintf(output, "};\n\n");
  write_glyphs(output, glyphs);
  write_bitmap(output, &bitmap);
  fprintf(output, "}  // namespace font_data\n");

  const int failed = fclose(output) != 0;
  free(bitmap.data);
  free(fontData);
  return failed;
}
