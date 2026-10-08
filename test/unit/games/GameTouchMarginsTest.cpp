#include <apps/GameTouchMargins.h>

#include <cstdint>
#include <cstdio>
#include <string>

#include "test_utils.h"

namespace {

using papyrix::Button;
using papyrix::games_app::touchMarginDirection;

struct ScreenSize {
  int width;
  int height;
  const char* name;
};

constexpr ScreenSize kScreenSizes[] = {
    {480, 800, "480x800"},
    {800, 480, "800x480"},
    {528, 792, "528x792"},
    {792, 528, "792x528"},
};

int marginWidth(int width) {
  const int scaled = width / 8;
  return scaled < 44 ? 44 : (scaled > 64 ? 64 : scaled);
}

int marginHeight(int height) {
  const int scaled = height / 12;
  return scaled < 44 ? 44 : (scaled > 64 ? 64 : scaled);
}

int canvasBottom(int height) { return height - 50 - 72; }

ui::touch::Point point(int x, int y) { return {static_cast<int16_t>(x), static_cast<int16_t>(y)}; }

Button directionAt(const ScreenSize& size, int x, int y) {
  return touchMarginDirection(point(x, y), size.width, size.height);
}

bool expectDirection(TestUtils::TestRunner& runner, Button expected, const ScreenSize& size, int x, int y,
                     const std::string& name) {
  return runner.expectEq(static_cast<int>(expected), static_cast<int>(directionAt(size, x, y)), name);
}

std::string caseName(const char* behavior, const ScreenSize& size) {
  char text[96];
  std::snprintf(text, sizeof(text), "%s at %s", behavior, size.name);
  return text;
}

}  // namespace

int main() {
  TestUtils::TestRunner runner("GameTouchMarginsTest");

  for (const auto& size : kScreenSizes) {
    const int topBand = marginHeight(size.height);
    const int bottom = canvasBottom(size.height);
    const int sideBand = marginWidth(size.width);
    const int midY = topBand + (bottom - 2 * topBand) / 2;

    expectDirection(runner, Button::Up, size, size.width / 2, 0, caseName("top edge returns Up", size));
    expectDirection(runner, Button::Down, size, size.width / 2, bottom - 1, caseName("bottom edge returns Down", size));
    expectDirection(runner, Button::Left, size, 0, midY, caseName("left edge returns Left", size));
    expectDirection(runner, Button::Right, size, size.width - 1, midY, caseName("right edge returns Right", size));

    expectDirection(runner, Button::Up, size, sideBand - 1, topBand - 1, caseName("top corner prefers Up", size));
    expectDirection(runner, Button::Down, size, sideBand - 1, bottom - topBand,
                    caseName("bottom corner prefers Down", size));

    expectDirection(runner, Button::Count, size, size.width / 2, bottom,
                    caseName("footer and dpad area are ignored", size));
    expectDirection(runner, Button::Count, size, size.width / 2, size.height - 86,
                    caseName("existing dpad row is ignored", size));
    expectDirection(runner, Button::Count, size, size.width / 2, size.height / 2,
                    caseName("center board is ignored", size));
  }

  {
    const ScreenSize size{480, 800, "480x800"};
    expectDirection(runner, Button::Count, size, -1, 10, "point left of screen is ignored");
    expectDirection(runner, Button::Count, size, size.width, 10, "point right of screen is ignored");
    expectDirection(runner, Button::Count, size, 10, -1, "point above screen is ignored");
    expectDirection(runner, Button::Count, size, 10, size.height, "point below screen is ignored");
  }

  {
    const ScreenSize shortScreen{480, 210, "480x210"};
    const ScreenSize narrowScreen{87, 800, "87x800"};
    expectDirection(runner, Button::Count, shortScreen, 240, 0, "height at guard limit is ignored");
    expectDirection(runner, Button::Count, narrowScreen, 0, 400, "too-narrow screen is ignored");
  }

  runner.printSummary();
  return runner.allPassed() ? 0 : 1;
}
