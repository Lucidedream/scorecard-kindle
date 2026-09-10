#include "HitTester.h"
#include "PgmCanvas.h"
#include "TouchInput.h"

#include <assert.h>
#include <stdio.h>

namespace {

TouchEvent classify(const uint64_t duration, const int upX, const int upY) {
  GestureClassifier classifier;
  classifier.begin(1000, 200, 300);
  return classifier.finish(1000 + duration, upX, upY);
}

void testGestures() {
  const TouchEvent tap = classify(100, 205, 304);
  assert(tap.kind == TouchEvent::Kind::Tap && tap.durationMs == 100);
  assert(classify(100, 340, 300).kind == TouchEvent::Kind::Tap);
  assert(classify(100, 341, 370).kind == TouchEvent::Kind::SwipeRight);
  assert(classify(100, 59, 230).kind == TouchEvent::Kind::SwipeLeft);
  assert(classify(450, 200, 300).kind == TouchEvent::Kind::Tap);
  const TouchEvent hold = classify(851, 239, 300);
  assert(hold.kind == TouchEvent::Kind::LongPress && hold.durationMs == 851);
  assert(classify(451, 240, 300).kind == TouchEvent::Kind::Tap);

  GestureClassifier wandering;
  wandering.begin(1000, 200, 300);
  wandering.update(250, 300);
  assert(wandering.finish(1500, 200, 300).kind == TouchEvent::Kind::Tap);
}

void testHitTester() {
  HitTester hitTester;
  hitTester.add({10, 20, 130, 140}, 7);
  hitTester.add({100, 100, 200, 200}, 8);
  assert(hitTester.hitTest(10, 20) == 7);
  assert(hitTester.hitTest(139, 159) == 7);
  assert(hitTester.hitTest(140, 160) == 8);
  assert(hitTester.hitTest(110, 110) == 7);
  assert(hitTester.hitTest(0, 0) == -1);
  hitTester.clear();
  assert(hitTester.hitTest(110, 110) == -1);
}

void testTextMetrics() {
  PgmCanvas canvas;
  assert(canvas.lineHeight(TextSize::Small) < canvas.lineHeight(TextSize::Body));
  assert(canvas.lineHeight(TextSize::Body) < canvas.lineHeight(TextSize::Display));
  assert(canvas.measureText("iiii", TextSize::Body) < canvas.measureText("WWWW", TextSize::Body));
  assert(canvas.measureText("W", TextSize::Small) < canvas.measureText("W", TextSize::Display));
  assert(canvas.measureText("Café £12", TextSize::Small) > 0);
  canvas.drawMonoText(10, 10, "425 YD · SI 3", TextSize::Small);
}

void testAntialiasing() {
  PgmCanvas canvas;
  canvas.clear();
  canvas.drawText(40, 40, "Rag", TextSize::Display);

  const char* path = "/tmp/m0_aa_test.pgm";
  assert(canvas.write(path));
  FILE* file = fopen(path, "rb");
  assert(file != nullptr);
  int width = 0;
  int height = 0;
  int maxValue = 0;
  assert(fscanf(file, "P5 %d %d %d", &width, &height, &maxValue) == 3);
  fgetc(file);

  bool seen[256] = {};
  for (long index = 0; index < static_cast<long>(width) * height; ++index) seen[fgetc(file) & 0xFF] = true;
  fclose(file);

  int distinct = 0;
  int midGray = 0;
  for (int value = 0; value < 256; ++value) {
    if (!seen[value]) continue;
    ++distinct;
    if (value > 24 && value < 231) ++midGray;
  }
  assert(distinct > 4);  // not a 1-bit blit
  assert(midGray > 0);   // glyph edges carry partial coverage
}

}  // namespace

int main() {
  testGestures();
  testHitTester();
  testTextMetrics();
  testAntialiasing();
  return 0;
}
