#include <Arduino.h>
#include <Display.h>
#include <GfxRenderer.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

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
constexpr int kEggRow = 3;

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

bool hasDrawnText(const std::string& expected) {
  const auto& calls = renderer.textCalls();
  return std::any_of(calls.begin(), calls.end(),
                     [&](const GfxRenderer::TextCall& call) { return call.text.find(expected) != std::string::npos; });
}

void resetApp(Core& core, unsigned long now = 100) {
  testSetManualMillis(now);
  core.settings = Settings{};
  renderer.clearCalls();
  enter(core);
  update(core);
}

void renderFresh(Core& core) {
  renderer.clearCalls();
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

Event tapBottomEdge() { return tap(renderer.getScreenWidth() / 2, renderer.getScreenHeight() - 140); }

Event tapLeftEdge() { return tap(8, renderer.getScreenHeight() / 2); }

Event tapRightEdge() { return tap(renderer.getScreenWidth() - 8, renderer.getScreenHeight() / 2); }

Event tapCenterBoard() { return tap(renderer.getScreenWidth() / 2, renderer.getScreenHeight() / 2); }

Event tapOutsideScreen() { return tap(-1, -1); }

Event tapEggQuadrant(int quadrant) {
  const int x = 16 + (renderer.getScreenWidth() - 32) * (quadrant % 2 * 2 + 1) / 4;
  const int y = 112 + (renderer.getScreenHeight() - 284) * (quadrant / 2 * 2 + 1) / 4;
  return tap(x, y);
}

void startSelectedGame(Core& core, int row, unsigned long now = 100) {
  resetApp(core, now);
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

bool linesStayOnScreen(const std::vector<GfxRenderer::LineCall>& lines) {
  for (const auto& line : lines) {
    if (line.x0 < 0 || line.x1 < 0 || line.y0 < 0 || line.y1 < 0) return false;
    if (line.x0 > renderer.getScreenWidth() || line.x1 > renderer.getScreenWidth()) return false;
    if (line.y0 > renderer.getScreenHeight() || line.y1 > renderer.getScreenHeight()) return false;
  }
  return true;
}

std::string basketLabel(const char* label) {
  std::string text("Basket: ");
  text += label;
  return text;
}

bool basketIs(const char* label) { return hasCenteredText(basketLabel(label)); }

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

void setEggPace(Core& core, bool turnBased, unsigned long now = 100) {
  startSelectedGame(core, kEggRow, now);
  handleEvent(core, press(Button::Center));
  update(core);
  renderFresh(core);
  const bool isTurnBased = hasDrawnText("Pace: Turn-based");
  if (isTurnBased != turnBased) {
    handleEvent(core, press(Button::Down));
    handleEvent(core, press(Button::Down));
    handleEvent(core, press(Button::Center));
    update(core);
  }
  handleEvent(core, press(Button::Back));
  update(core);
}

struct CapturedFrame {
  std::string name;
  int width;
  int height;
  uint8_t backgroundColor;
  std::vector<GfxRenderer::CenteredTextCall> centeredTextCalls;
  std::vector<GfxRenderer::TextCall> textCalls;
  std::vector<GfxRenderer::RectCall> drawRects;
  std::vector<GfxRenderer::RectCall> fillRects;
  std::vector<GfxRenderer::LineCall> lineCalls;
};

CapturedFrame captureFrame(const char* name) {
  return {name,
          renderer.getScreenWidth(),
          renderer.getScreenHeight(),
          renderer.clearColor(),
          renderer.centeredTextCalls(),
          renderer.textCalls(),
          renderer.drawRects(),
          renderer.fillRects(),
          renderer.lineCalls()};
}

void writeEscaped(FILE* file, const std::string& value) {
  std::fputc('"', file);
  for (unsigned char c : value) {
    switch (c) {
      case '\\':
        std::fputs("\\\\", file);
        break;
      case '"':
        std::fputs("\\\"", file);
        break;
      case '\n':
        std::fputs("\\n", file);
        break;
      case '\r':
        std::fputs("\\r", file);
        break;
      case '\t':
        std::fputs("\\t", file);
        break;
      default:
        if (c < 0x20) {
          std::fprintf(file, "\\u%04x", static_cast<unsigned>(c));
        } else {
          std::fputc(c, file);
        }
        break;
    }
  }
  std::fputc('"', file);
}

void writeCenteredText(FILE* file, const GfxRenderer::CenteredTextCall& call) {
  std::fprintf(file, "{\"font\":%d,\"y\":%d,\"black\":%s,\"style\":%d,\"text\":", call.fontId, call.y,
               call.black ? "true" : "false", static_cast<int>(call.style));
  writeEscaped(file, call.text);
  std::fputc('}', file);
}

void writeText(FILE* file, const GfxRenderer::TextCall& call) {
  std::fprintf(file, "{\"font\":%d,\"x\":%d,\"y\":%d,\"black\":%s,\"style\":%d,\"text\":", call.fontId, call.x, call.y,
               call.black ? "true" : "false", static_cast<int>(call.style));
  writeEscaped(file, call.text);
  std::fputc('}', file);
}

void writeRect(FILE* file, const GfxRenderer::RectCall& rect) {
  std::fprintf(file, "{\"x\":%d,\"y\":%d,\"w\":%d,\"h\":%d,\"black\":%s}", rect.x, rect.y, rect.w, rect.h,
               rect.color ? "true" : "false");
}

void writeLine(FILE* file, const GfxRenderer::LineCall& line) {
  std::fprintf(file, "{\"x0\":%d,\"y0\":%d,\"x1\":%d,\"y1\":%d,\"black\":%s}", line.x0, line.y0, line.x1, line.y1,
               line.color ? "true" : "false");
}

template <typename T, typename Writer>
void writeArray(FILE* file, const std::vector<T>& values, Writer writer) {
  std::fputc('[', file);
  for (size_t i = 0; i < values.size(); ++i) {
    if (i) std::fputc(',', file);
    writer(file, values[i]);
  }
  std::fputc(']', file);
}

void writeFramesJson(const std::vector<CapturedFrame>& frames, const char* outputPath) {
  const auto parent = std::filesystem::path(outputPath).parent_path();
  if (!parent.empty()) std::filesystem::create_directories(parent);
  FILE* file = std::fopen(outputPath, "wb");
  if (!file) return;
  std::fputs("{\"frames\":[", file);
  for (size_t i = 0; i < frames.size(); ++i) {
    if (i) std::fputc(',', file);
    const auto& frame = frames[i];
    std::fputs("{\"name\":", file);
    writeEscaped(file, frame.name);
    std::fprintf(file, ",\"width\":%d,\"height\":%d,\"backgroundColor\":%u,\"centeredText\":", frame.width,
                 frame.height, static_cast<unsigned>(frame.backgroundColor));
    writeArray(file, frame.centeredTextCalls, writeCenteredText);
    std::fputs(",\"drawText\":", file);
    writeArray(file, frame.textCalls, writeText);
    std::fputs(",\"drawRects\":", file);
    writeArray(file, frame.drawRects, writeRect);
    std::fputs(",\"fillRects\":", file);
    writeArray(file, frame.fillRects, writeRect);
    std::fputs(",\"drawLines\":", file);
    writeArray(file, frame.lineCalls, writeLine);
    std::fputc('}', file);
  }
  std::fputs("]}\n", file);
  std::fclose(file);
}

void exportScreens(Core& core, const char* outputPath) {
  std::vector<CapturedFrame> frames;
  constexpr ScreenSize kExportSizes[] = {
      {480, 800, "portrait"},
      {800, 480, "landscape"},
  };
  for (const auto& size : kExportSizes) {
    renderer.setScreenSize(size.width, size.height);
    resetApp(core);
    renderFresh(core);
    frames.push_back(captureFrame((std::string("nu-pogodi-chooser-") + size.name).c_str()));

    startSelectedGame(core, kEggRow);
    renderFresh(core);
    frames.push_back(captureFrame((std::string("nu-pogodi-initial-") + size.name).c_str()));

    handleEvent(core, tapDpad(3));
    update(core);
    testSetManualMillis(1200);
    update(core);
    renderFresh(core);
    frames.push_back(captureFrame((std::string("nu-pogodi-midgame-") + size.name).c_str()));

    handleEvent(core, press(Button::Center));
    update(core);
    renderFresh(core);
    frames.push_back(captureFrame((std::string("nu-pogodi-paused-") + size.name).c_str()));
  }
  writeFramesJson(frames, outputPath);
}

const char* screenOutputPath(int argc, char** argv) {
  for (int i = 1; i < argc; ++i) {
    const char* arg = argv[i] ? argv[i] : "";
    if (std::strcmp(arg, "--screens") == 0) {
      if (i + 1 < argc && argv[i + 1] && argv[i + 1][0] != '-') return argv[i + 1];
      return ".omx/games-preview/nu-pogodi-frames.json";
    }
    constexpr const char* kPrefix = "--screens=";
    if (std::strncmp(arg, kPrefix, std::strlen(kPrefix)) == 0) return arg + std::strlen(kPrefix);
  }
  return nullptr;
}

}  // namespace

int main(int argc, char** argv) {
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
    startSelectedGame(core, kEggRow);
    renderFresh(core);
    runner.expectTrue(hasCenteredText("Nu, Pogodi!"), "three downs then center launches Nu, Pogodi from chooser");
    runner.expectTrue(hasCenteredText("Score: 0   Misses: 0/3"), "Nu, Pogodi starts with zero score and misses");
    runner.expectTrue(basketIs("Upper left"), "Nu, Pogodi starts with upper-left basket selected");
  }

  {
    resetApp(core);
    handleEvent(core, press(Button::Up));
    handleEvent(core, press(Button::Center));
    renderFresh(core);
    runner.expectTrue(hasCenteredText("Nu, Pogodi!"), "chooser up wraps from first row to Nu, Pogodi");
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
    resetApp(core);
    handleEvent(core, tapChooserRow(3));
    renderFresh(core);
    runner.expectTrue(hasCenteredText("Nu, Pogodi!"), "touching fourth chooser row launches Nu, Pogodi");
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
    startSelectedGame(core, kEggRow);
    renderFresh(core);
    runner.expectTrue(hasDrawnText("Upper left"), "Nu, Pogodi direct pad labels upper-left basket");
    runner.expectTrue(hasDrawnText("Lower left"), "Nu, Pogodi direct pad labels lower-left basket");
    runner.expectTrue(hasDrawnText("Upper right"), "Nu, Pogodi direct pad labels upper-right basket");
    runner.expectTrue(hasDrawnText("Lower right"), "Nu, Pogodi direct pad labels lower-right basket");
  }

  {
    startSelectedGame(core, kEggRow);
    handleEvent(core, tapDpad(1));
    renderFresh(core);
    runner.expectTrue(basketIs("Lower left"), "Nu, Pogodi lower-left direct pad selects lane 1");
    handleEvent(core, tapDpad(2));
    renderFresh(core);
    runner.expectTrue(basketIs("Upper right"), "Nu, Pogodi upper-right direct pad selects lane 2");
    handleEvent(core, tapDpad(3));
    renderFresh(core);
    runner.expectTrue(basketIs("Lower right"), "Nu, Pogodi lower-right direct pad selects lane 3");
    handleEvent(core, tapDpad(0));
    renderFresh(core);
    runner.expectTrue(basketIs("Upper left"), "Nu, Pogodi upper-left direct pad selects lane 0");
  }

  {
    startSelectedGame(core, kEggRow);
    handleEvent(core, tapEggQuadrant(1));
    renderFresh(core);
    runner.expectTrue(basketIs("Upper right"), "Nu, Pogodi upper-right play quadrant selects lane 2");
    handleEvent(core, tapEggQuadrant(2));
    renderFresh(core);
    runner.expectTrue(basketIs("Lower left"), "Nu, Pogodi lower-left play quadrant selects lane 1");
    handleEvent(core, tapEggQuadrant(3));
    renderFresh(core);
    runner.expectTrue(basketIs("Lower right"), "Nu, Pogodi lower-right play quadrant selects lane 3");
    handleEvent(core, tapEggQuadrant(0));
    renderFresh(core);
    runner.expectTrue(basketIs("Upper left"), "Nu, Pogodi upper-left play quadrant selects lane 0");
  }

  {
    startSelectedGame(core, kEggRow);
    handleEvent(core, press(Button::Down));
    renderFresh(core);
    runner.expectTrue(basketIs("Lower left"), "Nu, Pogodi down button sets lower basket row");
    handleEvent(core, press(Button::Right));
    renderFresh(core);
    runner.expectTrue(basketIs("Lower right"), "Nu, Pogodi right button sets right basket side");
    handleEvent(core, press(Button::Up));
    renderFresh(core);
    runner.expectTrue(basketIs("Upper right"), "Nu, Pogodi up button sets upper basket row");
    handleEvent(core, press(Button::Left));
    renderFresh(core);
    runner.expectTrue(basketIs("Upper left"), "Nu, Pogodi left button sets left basket side");
  }

  {
    startSelectedGame(core, kEggRow);
    handleEvent(core, tapBottomEdge());
    renderFresh(core);
    runner.expectTrue(basketIs("Lower left"), "Nu, Pogodi bottom edge sets lower basket row");
    handleEvent(core, tapRightEdge());
    renderFresh(core);
    runner.expectTrue(basketIs("Lower right"), "Nu, Pogodi right edge sets right basket side");
    handleEvent(core, tapTopEdge());
    renderFresh(core);
    runner.expectTrue(basketIs("Upper right"), "Nu, Pogodi top edge sets upper basket row");
    handleEvent(core, tapLeftEdge());
    renderFresh(core);
    runner.expectTrue(basketIs("Upper left"), "Nu, Pogodi left edge sets left basket side");
  }

  {
    startSelectedGame(core, kEggRow);
    runner.expectTrue(handleEvent(core, press(Button::Center)), "Nu, Pogodi center opens pause menu");
    update(core);
    testSetManualMillis(10000);
    runner.expectFalse(update(core), "paused Nu, Pogodi does not advance on timer");
    renderFresh(core);
    runner.expectFalse(hasCenteredText("Catch eggs"), "Nu, Pogodi pause menu does not render gameplay hints");
  }

  {
    setEggPace(core, false);
    testSetManualMillis(500);
    handleEvent(core, press(Button::Center));
    update(core);
    handleEvent(core, press(Button::Center));
    update(core);
    testSetManualMillis(1599);
    runner.expectFalse(update(core), "resume resets Nu, Pogodi timer before the first full interval");
    testSetManualMillis(1600);
    runner.expectTrue(update(core), "Nu, Pogodi resumes timer after a full interval");
  }

  {
    setEggPace(core, false);
    testSetManualMillis(500);
    handleEvent(core, press(Button::Right));
    update(core);
    renderFresh(core);
    testSetManualMillis(1200);
    runner.expectTrue(update(core), "Nu, Pogodi basket switch and render do not reset egg timer");
  }

  {
    setEggPace(core, false, 0xFFFFFF00UL);
    testSetManualMillis(0xFFFFFF00UL + 500UL);
    runner.expectFalse(update(core), "Nu, Pogodi timer waits across uint32 rollover before interval");
    testSetManualMillis(0xFFFFFF00UL + 1100UL);
    runner.expectTrue(update(core), "Nu, Pogodi timer advances across uint32 rollover at interval");
  }

  {
    setEggPace(core, true);
    testSetManualMillis(10000);
    runner.expectFalse(update(core), "turn-based Nu, Pogodi does not advance on timer");
    runner.expectTrue(handleEvent(core, tapDpad(0)), "turn-based Nu, Pogodi same direct basket tap is consumed");
    runner.expectTrue(update(core), "turn-based Nu, Pogodi same direct basket tap advances once");
    runner.expectTrue(handleEvent(core, tapDpad(0)), "turn-based Nu, Pogodi repeat same basket tap is consumed");
    runner.expectTrue(update(core), "turn-based Nu, Pogodi repeat same basket tap advances once");
    runner.expectTrue(handleEvent(core, Event::buttonRepeat(Button::Right)), "Nu, Pogodi button repeat is consumed");
    runner.expectFalse(update(core), "Nu, Pogodi button repeat does not step");
    runner.expectTrue(handleEvent(core, tapOutsideScreen()), "Nu, Pogodi outside tap is consumed");
    runner.expectFalse(update(core), "Nu, Pogodi outside tap does not step");
  }

  {
    startSelectedGame(core, kEggRow);
    core.settings.frontButtonLayout = Settings::FrontLRBC;
    runner.expectTrue(handleEvent(core, tapButtonBar(2)),
                      "Nu, Pogodi LRBC visual back button keeps footer priority while playing");
    renderFresh(core);
    runner.expectTrue(hasCenteredText("Games"), "Nu, Pogodi LRBC footer back returns to chooser");
  }

  {
    startSelectedGame(core, kEggRow);
    core.settings.frontButtonLayout = Settings::FrontLRBC;
    runner.expectTrue(handleEvent(core, tapButtonBar(3)),
                      "Nu, Pogodi LRBC visual center button keeps footer priority while playing");
    update(core);
    testSetManualMillis(10000);
    runner.expectFalse(update(core), "Nu, Pogodi LRBC footer center pauses instead of selecting basket");
  }

  {
    startSelectedGame(core, kEggRow);
    runner.expectTrue(handleEvent(core, tapButtonBar(0)), "Nu, Pogodi normal visual back button keeps footer priority");
    renderFresh(core);
    runner.expectTrue(hasCenteredText("Games"), "Nu, Pogodi normal footer back returns to chooser");
  }

  {
    startSelectedGame(core, kEggRow);
    runner.expectTrue(handleEvent(core, tapButtonBar(1)),
                      "Nu, Pogodi normal visual center button keeps footer priority while playing");
    update(core);
    testSetManualMillis(10000);
    runner.expectFalse(update(core), "Nu, Pogodi normal footer center pauses instead of selecting basket");
  }

  {
    startSelectedGame(core, kEggRow);
    handleEvent(core, press(Button::Center));
    update(core);
    handleEvent(core, press(Button::Down));
    handleEvent(core, press(Button::Center));
    renderFresh(core);
    runner.expectTrue(hasCenteredText("Nu, Pogodi!"), "Nu, Pogodi pause menu new game restarts current game");
    runner.expectTrue(hasCenteredText("Score: 0   Misses: 0/3"), "Nu, Pogodi restart resets scoreboard");
    runner.expectTrue(basketIs("Upper left"), "Nu, Pogodi restart resets basket lane");
  }

  {
    startSelectedGame(core, 1);
    renderFresh(core);
    runner.expectTrue(!renderer.fillRects().empty(), "snake render draws filled board cells");
    runner.expectTrue(rectsStayOnScreen(renderer.fillRects()), "snake filled cells stay inside screen bounds");
  }

  for (const auto& size : kScreenSizes) {
    renderer.setScreenSize(size.width, size.height);
    for (int gameRow = 0; gameRow < 4; ++gameRow) {
      startSelectedGame(core, gameRow);
      renderFresh(core);
      runner.expectTrue(!renderer.drawRects().empty(), testName("render outlines", gameRow, size.name));
      runner.expectTrue(rectsStayOnScreen(renderer.drawRects()),
                        testName("outlined rects stay inside", gameRow, size.name));
      runner.expectTrue(rectsStayOnScreen(renderer.fillRects()),
                        testName("filled rects stay inside", gameRow, size.name));
      runner.expectTrue(linesStayOnScreen(renderer.lineCalls()), testName("lines stay inside", gameRow, size.name));
    }
  }

  runner.printSummary();
  if (const char* screensPath = screenOutputPath(argc, argv)) {
    exportScreens(core, screensPath);
    std::printf("Wrote Nu, Pogodi screen frames to %s\n", screensPath);
  }
  testUseRealtimeMillis();
  return runner.allPassed() ? 0 : 1;
}
