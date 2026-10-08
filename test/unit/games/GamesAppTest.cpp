#include <Arduino.h>
#include <Display.h>
#include <GfxRenderer.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <string>

#include "apps/GamesApp.h"
#include "content/ContentHandle.h"
#include "core/Core.h"
#include "test_utils.h"
#include "ui/TouchLayout.h"

namespace papyrix {
ContentMetadata ContentHandle::emptyMetadata_ = {};
ContentHandle::ContentHandle() : type(ContentType::None) {}
ContentHandle::~ContentHandle() {}
}  // namespace papyrix

papyrix::hal::Display display;
GfxRenderer renderer(display);

namespace {

using papyrix::Button;
using papyrix::Core;
using papyrix::Event;
using papyrix::Settings;
using papyrix::games_app::enter;
using papyrix::games_app::handleEvent;
using papyrix::games_app::render;
using papyrix::games_app::update;

constexpr int kChooserFirstRowY = 102;

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

bool hasCenteredText(const std::string& expected) {
  const auto& calls = renderer.centeredTextCalls();
  return std::any_of(calls.begin(), calls.end(), [&](const GfxRenderer::CenteredTextCall& call) {
    return call.text.find(expected) != std::string::npos;
  });
}

void resetApp(Core& core, unsigned long now = 100) {
  testSetManualMillis(now);
  core.settings = Settings{};
  renderer.clearCenteredTextCalls();
  renderer.clearRects();
  enter(core);
  update(core);
}

void renderFresh(Core& core) {
  renderer.clearCenteredTextCalls();
  renderer.clearRects();
  render(core);
}

Event press(Button button) { return Event::buttonPress(button); }

Event tap(int x, int y) { return Event::tap({static_cast<int16_t>(x), static_cast<int16_t>(y)}); }

Event tapButtonBar(int visualIndex) {
  const auto rect = ui::touch::buttonBarButtonRect(visualIndex, renderer.getScreenWidth(), renderer.getScreenHeight());
  return tap(rect.x + rect.width / 2, rect.y + rect.height / 2);
}

Event tapChooserRow(int row) { return tap(20, kChooserFirstRowY + row * 44); }

Event tapDpad(int visualIndex) {
  const int w = renderer.getScreenWidth();
  const int h = renderer.getScreenHeight();
  return tap(visualIndex * w / 4 + w / 8, h - 50 - 72 + 36);
}

Event tapTopEdge() { return tap(renderer.getScreenWidth() / 2, 8); }

Event tapLeftEdge() { return tap(8, renderer.getScreenHeight() / 2); }

Event tapCenterBoard() { return tap(renderer.getScreenWidth() / 2, renderer.getScreenHeight() / 2); }

void startSelectedGame(Core& core, int row, unsigned long now = 100) {
  testSetManualMillis(now);
  resetApp(core);
  for (int i = 0; i < row; ++i) {
    handleEvent(core, press(Button::Down));
  }
  handleEvent(core, press(Button::Center));
  update(core);
}

bool rectsStayOnScreen(const std::vector<GfxRenderer::RectCall>& rects) {
  for (const auto& rect : rects) {
    if (rect.w <= 0 || rect.h <= 0) return false;
    if (rect.x < 0 || rect.y < 0) return false;
    if (rect.x + rect.w > renderer.getScreenWidth()) return false;
    if (rect.y + rect.h > renderer.getScreenHeight()) return false;
  }
  return true;
}

void toggleSnakePace(Core& core) {
  handleEvent(core, press(Button::Center));
  update(core);
  handleEvent(core, press(Button::Down));
  handleEvent(core, press(Button::Down));
  handleEvent(core, press(Button::Center));
  update(core);
  handleEvent(core, press(Button::Back));
  update(core);
}

void setTurnBasedSnake(Core& core) {
  startSelectedGame(core, 1);
  toggleSnakePace(core);
  testSetManualMillis(10000);
  if (update(core)) {
    toggleSnakePace(core);
    testSetManualMillis(20000);
    update(core);
  }
}

std::string testName(const char* prefix, int gameRow, const char* sizeName) {
  char text[96];
  std::snprintf(text, sizeof(text), "%s game %d at %s", prefix, gameRow, sizeName);
  return text;
}

bool rectsEqual(const std::vector<GfxRenderer::RectCall>& lhs, const std::vector<GfxRenderer::RectCall>& rhs) {
  if (lhs.size() != rhs.size()) return false;
  for (size_t i = 0; i < lhs.size(); ++i) {
    if (lhs[i].x != rhs[i].x || lhs[i].y != rhs[i].y || lhs[i].w != rhs[i].w || lhs[i].h != rhs[i].h ||
        lhs[i].color != rhs[i].color) {
      return false;
    }
  }
  return true;
}

std::vector<GfxRenderer::RectCall> fallingRectsAfterInput(Core& core, Event input) {
  startSelectedGame(core, 2);
  handleEvent(core, input);
  renderFresh(core);
  return renderer.fillRects();
}

}  // namespace

int main() {
  TestUtils::TestRunner runner("GamesAppTest");
  Core core;

  {
    resetApp(core);
    runner.expectFalse(handleEvent(core, press(Button::Back)), "chooser back returns false so launcher exits");
  }

  {
    resetApp(core);
    handleEvent(core, press(Button::Center));
    renderFresh(core);
    runner.expectTrue(hasCenteredText("2048"), "center launches 2048 from chooser");
  }

  {
    startSelectedGame(core, 1);
    renderFresh(core);
    runner.expectTrue(hasCenteredText("Snake"), "down then center launches snake from chooser");
  }

  {
    startSelectedGame(core, 2);
    renderFresh(core);
    runner.expectTrue(hasCenteredText("Falling Blocks"), "two downs then center launches falling blocks from chooser");
  }

  {
    resetApp(core);
    handleEvent(core, tapChooserRow(0));
    renderFresh(core);
    runner.expectTrue(hasCenteredText("2048"), "touching first chooser row launches 2048");
  }

  {
    resetApp(core);
    handleEvent(core, tapChooserRow(1));
    renderFresh(core);
    runner.expectTrue(hasCenteredText("Snake"), "touching second chooser row launches snake");
  }

  {
    resetApp(core);
    handleEvent(core, tapChooserRow(2));
    renderFresh(core);
    runner.expectTrue(hasCenteredText("Falling Blocks"), "touching third chooser row launches falling blocks");
  }

  {
    startSelectedGame(core, 1);
    runner.expectTrue(handleEvent(core, press(Button::Center)), "center opens pause menu while playing");
    update(core);
    testSetManualMillis(10000);
    runner.expectFalse(update(core), "paused snake does not advance on timer");
  }

  {
    startSelectedGame(core, 1);
    testSetManualMillis(500);
    handleEvent(core, press(Button::Center));
    update(core);
    handleEvent(core, press(Button::Center));
    update(core);
    testSetManualMillis(1399);
    runner.expectFalse(update(core), "resume resets snake timer before the first full interval");
  }

  {
    setTurnBasedSnake(core);
    testSetManualMillis(10000);
    runner.expectFalse(update(core), "turn-based snake does not advance on timer");
  }

  {
    setTurnBasedSnake(core);
    runner.expectTrue(handleEvent(core, press(Button::Left)), "turn-based snake reverse input is consumed");
    runner.expectFalse(update(core), "turn-based snake rejected reverse input does not redraw");
    runner.expectTrue(handleEvent(core, press(Button::Right)), "turn-based snake same direction input is consumed");
    runner.expectTrue(update(core), "turn-based snake same direction input steps and redraws");
  }

  {
    setTurnBasedSnake(core);
    runner.expectTrue(handleEvent(core, tapLeftEdge()), "turn-based snake left edge reverse input is consumed");
    runner.expectFalse(update(core), "turn-based snake rejected left edge reverse input does not redraw");
  }

  {
    setTurnBasedSnake(core);
    runner.expectTrue(handleEvent(core, tapTopEdge()), "turn-based snake top edge input is consumed");
    runner.expectTrue(update(core), "turn-based snake top edge input steps and redraws");
  }

  {
    startSelectedGame(core, 2);
    runner.expectTrue(handleEvent(core, press(Button::Center)), "center opens blocks pause menu");
    update(core);
    testSetManualMillis(10000);
    runner.expectFalse(update(core), "paused blocks does not advance on timer");
  }

  {
    startSelectedGame(core, 2);
    handleEvent(core, press(Button::Center));
    update(core);
    handleEvent(core, press(Button::Down));
    handleEvent(core, press(Button::Center));
    renderFresh(core);
    runner.expectTrue(hasCenteredText("Falling Blocks"), "pause menu new game restarts the current game");
  }

  {
    const auto buttonUpRects = fallingRectsAfterInput(core, press(Button::Up));
    const auto topEdgeRects = fallingRectsAfterInput(core, tapTopEdge());
    runner.expectTrue(rectsEqual(buttonUpRects, topEdgeRects), "falling blocks top edge matches up-button rotation");
  }

  {
    startSelectedGame(core, 1);
    runner.expectTrue(handleEvent(core, press(Button::Back)), "back while playing is consumed");
    renderFresh(core);
    runner.expectTrue(hasCenteredText("Games"), "back while playing returns to chooser");
  }

  {
    resetApp(core);
    runner.expectTrue(handleEvent(core, tapTopEdge()), "chooser consumes top edge tap");
    renderFresh(core);
    runner.expectTrue(hasCenteredText("Games"), "chooser top edge tap does not launch gameplay");
  }

  {
    startSelectedGame(core, 1);
    handleEvent(core, press(Button::Center));
    update(core);
    runner.expectTrue(handleEvent(core, tapTopEdge()), "pause menu consumes top edge tap");
    testSetManualMillis(10000);
    runner.expectFalse(update(core), "pause menu top edge tap does not resume gameplay");
  }

  {
    resetApp(core);
    core.settings.frontButtonLayout = Settings::FrontLRBC;
    runner.expectFalse(handleEvent(core, tapButtonBar(2)), "LRBC visual back button exits chooser");
  }

  {
    startSelectedGame(core, 1);
    core.settings.frontButtonLayout = Settings::FrontLRBC;
    runner.expectTrue(handleEvent(core, tapButtonBar(2)),
                      "LRBC visual back button keeps footer priority while playing");
    renderFresh(core);
    runner.expectTrue(hasCenteredText("Games"), "LRBC footer back returns from game instead of margin input");
  }

  {
    startSelectedGame(core, 1);
    core.settings.frontButtonLayout = Settings::FrontLRBC;
    runner.expectTrue(handleEvent(core, tapButtonBar(3)),
                      "LRBC visual center button keeps footer priority while playing");
    update(core);
    testSetManualMillis(10000);
    runner.expectFalse(update(core), "LRBC footer center pauses instead of margin input");
  }

  {
    startSelectedGame(core, 1);
    runner.expectTrue(handleEvent(core, tapDpad(0)), "playing touch dpad up is consumed");
  }

  {
    startSelectedGame(core, 2);
    runner.expectTrue(handleEvent(core, tapDpad(1)), "playing touch dpad down is consumed");
  }

  {
    startSelectedGame(core, 1);
    runner.expectTrue(handleEvent(core, tapCenterBoard()), "playing board center tap is consumed");
    runner.expectFalse(update(core), "playing board center tap does not trigger gameplay input");
  }

  {
    startSelectedGame(core, 1);
    renderFresh(core);
    runner.expectTrue(!renderer.fillRects().empty(), "snake render draws filled board cells");
    runner.expectTrue(rectsStayOnScreen(renderer.fillRects()), "snake filled cells stay inside screen bounds");
  }

  for (const auto& size : kScreenSizes) {
    renderer.setScreenSize(size.width, size.height);
    for (int gameRow = 0; gameRow < 3; ++gameRow) {
      startSelectedGame(core, gameRow);
      renderFresh(core);
      runner.expectTrue(!renderer.drawRects().empty(), testName("render outlines", gameRow, size.name));
      runner.expectTrue(rectsStayOnScreen(renderer.drawRects()),
                        testName("outlined rects stay inside", gameRow, size.name));
      runner.expectTrue(rectsStayOnScreen(renderer.fillRects()),
                        testName("filled rects stay inside", gameRow, size.name));
    }
  }

  runner.printSummary();
  testUseRealtimeMillis();
  return runner.allPassed() ? 0 : 1;
}
