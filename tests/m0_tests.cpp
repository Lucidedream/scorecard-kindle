#include "HitTester.h"
#include "PgmCanvas.h"
#include "TouchInput.h"

#include <assert.h>

namespace {

TouchEvent classify(const uint64_t duration, const int upX, const int upY) {
  GestureClassifier classifier;
  classifier.begin(1000, 200, 300);
  return classifier.finish(1000 + duration, upX, upY);
}

void testGestures() {
  assert(classify(100, 205, 304).kind == TouchEvent::Kind::Tap);
  assert(classify(100, 340, 300).kind == TouchEvent::Kind::Tap);
  assert(classify(100, 341, 370).kind == TouchEvent::Kind::SwipeRight);
  assert(classify(100, 59, 230).kind == TouchEvent::Kind::SwipeLeft);
  assert(classify(450, 200, 300).kind == TouchEvent::Kind::Tap);
  assert(classify(451, 239, 300).kind == TouchEvent::Kind::LongPress);
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
  assert(canvas.measureText("Café £12", TextSize::Small) > 0);
}

}  // namespace

int main() {
  testGestures();
  testHitTester();
  testTextMetrics();
  return 0;
}
