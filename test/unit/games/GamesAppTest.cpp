#include <Arduino.h>
#include <Display.h>
#include <GfxRenderer.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

#include "apps/GameModels.h"
#include "apps/GamesApp.h"
#include "apps/NuPogodiArtwork.h"
#include "apps/SolitaireModel.h"
#include "apps/SolitaireView.h"
#include "content/ContentHandle.h"
#include "core/Core.h"
#include "test_utils.h"
#include "ui/TouchLayout.h"

namespace papyrix {
ContentMetadata ContentHandle::emptyMetadata_ = {};
ContentHandle::ContentHandle() : type(ContentType::None) {}
ContentHandle::~ContentHandle() {}
}  // namespace papyrix

namespace papyrix::games {

struct SolitaireTestAccess {
  static void clear(Solitaire& game) {
    game.clearState();
    game.historyNext_ = 0;
    game.historyCount_ = 0;
  }

  static void setPile(Solitaire& game, int pile, const uint8_t* cards, int count) {
    game.state_.counts[pile] = static_cast<uint8_t>(count);
    game.state_.faceMask[pile] = 0;
    for (int i = 0; i < count; ++i) {
      game.state_.cards[pile][i] = cards[i];
      game.setFaceUp(game.state_, pile, i, true);
    }
  }
};

}  // namespace papyrix::games

papyrix::hal::Display display;
GfxRenderer renderer(display);

namespace {

using papyrix::Button;
using papyrix::Core;
using papyrix::Event;
using papyrix::Settings;
using papyrix::games::Game2048;
using papyrix::games::Solitaire;
using papyrix::games_app::enter;
using papyrix::games_app::handleEvent;
using papyrix::games_app::render;
using papyrix::games_app::update;

constexpr int kChooserFirstRowY = 102;
constexpr int kEggRow = 3;
constexpr int kSolitaireRow = 4;

struct ScreenSize {
  int width;
  int height;
  const char* name;
};

struct TestPoint {
  int x;
  int y;
};

struct TestRect {
  int x;
  int y;
  int w;
  int h;
};

struct SolitaireLayoutForTest {
  int w;
  int h;
  int cardW;
  int cardH;
  int gap;
  int left;
  int topY;
  int tableauY;
  int boardBottom;
  int controlsY;
};

struct SolitaireMoveForTest {
  int draws;
  int fromPile;
  int fromIndex;
  int toPile;
};

struct FinishDialogForTest {
  TestRect panel;
  TestRect close;
  TestRect again;
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

bool hasExactDrawnText(const std::string& expected) {
  const auto& calls = renderer.textCalls();
  return std::any_of(calls.begin(), calls.end(),
                     [&](const GfxRenderer::TextCall& call) { return call.text == expected; });
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

Event tapSolitaireControl(int index) {
  const int w = renderer.getScreenWidth();
  const int h = renderer.getScreenHeight();
  return tap(index * w / 3 + w / 6, h - 50 - 72 + 36);
}

Event tapPoint(TestPoint point) { return tap(point.x, point.y); }

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

bool testRectsStayOnScreen(const std::vector<TestRect>& rects) {
  for (const auto& rect : rects) {
    if (rect.w <= 0 || rect.h <= 0) return false;
    if (rect.x < 0 || rect.y < 0) return false;
    if (rect.x + rect.w > renderer.getScreenWidth()) return false;
    if (rect.y + rect.h > renderer.getScreenHeight()) return false;
  }
  return true;
}

std::vector<GfxRenderer::RectCall> controlRects() {
  std::vector<GfxRenderer::RectCall> rects;
  const int y = renderer.getScreenHeight() - 50 - 72 + 4;
  for (const auto& rect : renderer.drawRects()) {
    if (rect.y == y && rect.h == 64) rects.push_back(rect);
  }
  return rects;
}

bool hasWideMiddleControl(const std::vector<GfxRenderer::RectCall>& rects) {
  if (rects.size() != 3) return false;
  return rects[1].w >= rects[0].w * 2 - 8 && rects[1].w >= rects[2].w * 2 - 8;
}

bool linesStayOnScreen(const std::vector<GfxRenderer::LineCall>& lines) {
  for (const auto& line : lines) {
    if (line.x0 < 0 || line.x1 < 0 || line.y0 < 0 || line.y1 < 0) return false;
    if (line.x0 > renderer.getScreenWidth() || line.x1 > renderer.getScreenWidth()) return false;
    if (line.y0 > renderer.getScreenHeight() || line.y1 > renderer.getScreenHeight()) return false;
  }
  return true;
}

FinishDialogForTest finishDialogForTest() {
  const int w = renderer.getScreenWidth();
  const int h = renderer.getScreenHeight();
  const int panelW = std::min(420, w - 32);
  const int panelH = 180;
  const int panelX = (w - panelW) / 2;
  const int panelY = (h - panelH) / 2;
  const int buttonW = (panelW - 48) / 2;
  return {{panelX, panelY, panelW, panelH},
          {panelX + 16, panelY + 112, buttonW, 52},
          {panelX + 32 + buttonW, panelY + 112, buttonW, 52}};
}

bool hasRect(const TestRect& expected) {
  const auto& rects = renderer.drawRects();
  return std::any_of(rects.begin(), rects.end(), [&](const GfxRenderer::RectCall& rect) {
    return rect.x == expected.x && rect.y == expected.y && rect.w == expected.w && rect.h == expected.h;
  });
}

Event tapRectCenter(const TestRect& rect) { return tap(rect.x + rect.w / 2, rect.y + rect.h / 2); }

SolitaireLayoutForTest solitaireLayoutForTest() {
  const int w = renderer.getScreenWidth();
  const int h = renderer.getScreenHeight();
  const int gap = w < 520 ? 5 : 8;
  const int cardW = std::max(42, std::min(w < h ? 66 : 72, (w - 16 - gap * 6) / 7));
  const int cardH = std::max(58, std::min(h < 550 ? 64 : 88, cardW * 4 / 3));
  const int total = cardW * 7 + gap * 6;
  return {w, h, cardW, cardH, gap, (w - total) / 2, 80, h < 550 ? 174 : 180, h - 156, h - 50 - 72};
}

TestRect solitaireTopRectForTest(int slot) {
  const auto l = solitaireLayoutForTest();
  return {l.left + slot * (l.cardW + l.gap), l.topY, l.cardW, l.cardH};
}

TestRect solitaireColumnRectForTest(int col) {
  const auto l = solitaireLayoutForTest();
  return {l.left + col * (l.cardW + l.gap), l.tableauY, l.cardW, l.boardBottom - l.tableauY};
}

TestPoint centerOf(TestRect rect) { return {rect.x + rect.w / 2, rect.y + rect.h / 2}; }

int firstFaceUpForTest(const Solitaire& game, int pile) {
  const int count = game.count(pile);
  for (int i = 0; i < count; ++i) {
    if (game.faceUp(pile, i)) return i;
  }
  return count;
}

TestPoint solitaireCardPointForTest(const Solitaire& game, int pile, int index) {
  if (pile == Solitaire::WASTE) return centerOf(solitaireTopRectForTest(1));
  if (pile >= Solitaire::FOUNDATION_FIRST && pile < Solitaire::TABLEAU_FIRST) {
    return centerOf(solitaireTopRectForTest(pile - Solitaire::FOUNDATION_FIRST + 3));
  }
  const auto l = solitaireLayoutForTest();
  const int col = pile - Solitaire::TABLEAU_FIRST;
  const auto column = solitaireColumnRectForTest(col);
  const int first = firstFaceUpForTest(game, pile);
  const int pitch = std::max(22, renderer.getLineHeight(0) + 4);
  const int startY = column.y + first * (l.h < 550 ? 6 : 12);
  return {column.x + column.w / 2, startY + std::max(0, index - first) * pitch + l.cardH / 2};
}

TestPoint solitaireTargetPointForTest(int pile) {
  if (pile >= Solitaire::FOUNDATION_FIRST && pile < Solitaire::TABLEAU_FIRST) {
    return centerOf(solitaireTopRectForTest(pile - Solitaire::FOUNDATION_FIRST + 3));
  }
  if (pile >= Solitaire::TABLEAU_FIRST && pile < Solitaire::PILE_COUNT) {
    const auto column = solitaireColumnRectForTest(pile - Solitaire::TABLEAU_FIRST);
    return {column.x + column.w / 2, column.y + 8};
  }
  return centerOf(solitaireTopRectForTest(1));
}

bool findLegalSolitaireMove(SolitaireMoveForTest& out) {
  Solitaire game;
  game.reset(100);
  for (int draws = 0; draws <= 8; ++draws) {
    for (int fromPile = Solitaire::WASTE; fromPile < Solitaire::PILE_COUNT; ++fromPile) {
      const int first =
          fromPile >= Solitaire::TABLEAU_FIRST ? firstFaceUpForTest(game, fromPile) : game.count(fromPile) - 1;
      for (int fromIndex = std::max(0, first); fromIndex < game.count(fromPile); ++fromIndex) {
        if (!game.faceUp(fromPile, fromIndex)) continue;
        for (int toPile = Solitaire::FOUNDATION_FIRST; toPile < Solitaire::PILE_COUNT; ++toPile) {
          Solitaire copy = game;
          if (copy.move(fromPile, fromIndex, toPile)) {
            out = {draws, fromPile, fromIndex, toPile};
            return true;
          }
        }
      }
    }
    if (!game.draw()) break;
  }
  return false;
}

uint8_t solitaireCard(int suit, int rank) { return static_cast<uint8_t>(suit * 13 + rank - 1); }

Solitaire winningSolitaireForTest() {
  Solitaire game;
  papyrix::games::SolitaireTestAccess::clear(game);
  for (int suit = 0; suit < 4; ++suit) {
    uint8_t cards[13] = {};
    for (int rank = 1; rank <= 13; ++rank) cards[rank - 1] = solitaireCard(suit, rank);
    papyrix::games::SolitaireTestAccess::setPile(game, Solitaire::FOUNDATION_FIRST + suit, cards, 13);
  }
  return game;
}

void load2048BoardForTest(Core& core, const Game2048& board) {
  startSelectedGame(core, 0);
#ifdef TEST_BUILD
  papyrix::games_app::load2048ForTest(board);
#endif
  update(core);
}

Game2048 won2048ForTest() {
  Game2048 game;
  game.clear();
  game.setTile(0, 0, 2048);
  return game;
}

Game2048 blocked2048ForTest() {
  Game2048 game;
  game.clear();
  for (int row = 0; row < Game2048::SIZE; ++row) {
    for (int col = 0; col < Game2048::SIZE; ++col) {
      game.setTile(row, col, ((row + col) % 2 == 0) ? 2 : 4);
    }
  }
  return game;
}

void openSolitaireWinDialog(Core& core) {
  startSelectedGame(core, kSolitaireRow);
#ifdef TEST_BUILD
  papyrix::games_app::solitaire_view::loadForTest(winningSolitaireForTest());
#endif
  update(core);
}

bool renderShows(Core& core, const std::string& text) {
  renderFresh(core);
  return hasCenteredText(text) || hasDrawnText(text);
}

bool advanceUntilCentered(Core& core, const std::string& text, int steps, unsigned long stepMs) {
  unsigned long now = 100;
  for (int i = 0; i < steps; ++i) {
    now += stepMs;
    testSetManualMillis(now);
    update(core);
    renderFresh(core);
    if (hasCenteredText(text)) return true;
  }
  return false;
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

#if PAPYRIX_NU_ORIGINAL_ART_AVAILABLE
int blackPixels(const papyrix::games::art::Bitmap& bitmap) {
  if (!bitmap.data || bitmap.width == 0 || bitmap.height == 0) return 0;
  const int rowBytes = (bitmap.width + 7) / 8;
  int count = 0;
  for (int y = 0; y < bitmap.height; ++y) {
    for (int x = 0; x < bitmap.width; ++x) {
      if ((bitmap.data[y * rowBytes + x / 8] & (0x80 >> (x & 7))) == 0) ++count;
    }
  }
  return count;
}

uint32_t bitmapFingerprint(const papyrix::games::art::Bitmap& bitmap) {
  uint32_t hash = 2166136261u;
  const auto mix = [&](uint32_t value) {
    hash ^= value;
    hash *= 16777619u;
  };
  mix(static_cast<uint16_t>(bitmap.x));
  mix(static_cast<uint16_t>(bitmap.y));
  mix(bitmap.width);
  mix(bitmap.height);
  const int rowBytes = (bitmap.width + 7) / 8;
  for (int i = 0; bitmap.data && i < rowBytes * bitmap.height; ++i) mix(bitmap.data[i]);
  return hash;
}

bool bitmapFitsArtwork(const papyrix::games::art::Bitmap& bitmap, const papyrix::games::art::Artwork& artwork) {
  return bitmap.data && bitmap.width > 0 && bitmap.height > 0 && bitmap.x >= 0 && bitmap.y >= 0 &&
         bitmap.x + bitmap.width <= artwork.width && bitmap.y + bitmap.height <= artwork.height;
}

bool aspectPreservedWithinOnePixel(uint16_t width, uint16_t height) {
  constexpr double kSourceWidth = 1570.2845;
  constexpr double kSourceHeight = 989.1816;
  const int expectedHeight = static_cast<int>(std::lround(width * kSourceHeight / kSourceWidth));
  const int expectedWidth = static_cast<int>(std::lround(height * kSourceWidth / kSourceHeight));
  return std::abs(static_cast<int>(height) - expectedHeight) <= 1 ||
         std::abs(static_cast<int>(width) - expectedWidth) <= 1;
}

void expectInkAndBounds(TestUtils::TestRunner& runner, const papyrix::games::art::Artwork& artwork,
                        const papyrix::games::art::Bitmap& bitmap, const std::string& name) {
  runner.expectTrue(bitmapFitsArtwork(bitmap, artwork), name + " bitmap stays inside artwork");
  runner.expectTrue(blackPixels(bitmap) > 0, name + " bitmap has native ink");
}
#endif

std::vector<GfxRenderer::RectCall> fallingRectsAfterInput(Core& core, Event input) {
  startSelectedGame(core, 2);
  handleEvent(core, input);
  renderFresh(core);
  return renderer.drawRects();
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

bool openSnakeGameOverDialog(Core& core) {
  setTurnBasedSnake(core);
  for (int i = 0; i < 20; ++i) {
    handleEvent(core, press(Button::Right));
    update(core);
    renderFresh(core);
    if (hasCenteredText("Game over")) return true;
  }
  return false;
}

bool openEggGameOverDialog(Core& core) {
  setEggPace(core, false);
  return advanceUntilCentered(core, "Game over", 200, 1100);
}

bool openBlocksGameOverDialog(Core& core) {
  startSelectedGame(core, 2);
  for (int i = 0; i < 500; ++i) {
    testSetManualMillis(100 + (i + 1) * 2000UL);
    update(core);
    renderFresh(core);
    if (hasCenteredText("Game over")) return true;
  }
  return false;
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
  std::vector<GfxRenderer::DrawCall> operations;
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
          renderer.lineCalls(),
          renderer.operations()};
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

const char* operationKindName(GfxRenderer::DrawCall::Kind kind) {
  switch (kind) {
    case GfxRenderer::DrawCall::Kind::Rect:
      return "rect";
    case GfxRenderer::DrawCall::Kind::Fill:
      return "fill";
    case GfxRenderer::DrawCall::Kind::Line:
      return "line";
    case GfxRenderer::DrawCall::Kind::Text:
      return "text";
    case GfxRenderer::DrawCall::Kind::Centered:
      return "centered";
  }
  return "unknown";
}

void writeOperation(FILE* file, const GfxRenderer::DrawCall& call) {
  std::fprintf(file, "{\"kind\":\"%s\"", operationKindName(call.kind));
  switch (call.kind) {
    case GfxRenderer::DrawCall::Kind::Rect:
    case GfxRenderer::DrawCall::Kind::Fill:
      std::fprintf(file, ",\"x\":%d,\"y\":%d,\"w\":%d,\"h\":%d,\"black\":%s", call.x, call.y, call.w, call.h,
                   call.black ? "true" : "false");
      break;
    case GfxRenderer::DrawCall::Kind::Line:
      std::fprintf(file, ",\"x0\":%d,\"y0\":%d,\"x1\":%d,\"y1\":%d,\"black\":%s", call.x, call.y, call.x1, call.y1,
                   call.black ? "true" : "false");
      break;
    case GfxRenderer::DrawCall::Kind::Text:
      std::fprintf(file, ",\"font\":%d,\"x\":%d,\"y\":%d,\"black\":%s,\"style\":%d,\"text\":", call.fontId, call.x,
                   call.y, call.black ? "true" : "false", static_cast<int>(call.style));
      writeEscaped(file, call.text);
      break;
    case GfxRenderer::DrawCall::Kind::Centered:
      std::fprintf(file, ",\"font\":%d,\"y\":%d,\"black\":%s,\"style\":%d,\"text\":", call.fontId, call.y,
                   call.black ? "true" : "false", static_cast<int>(call.style));
      writeEscaped(file, call.text);
      break;
  }
  std::fputc('}', file);
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
    std::fputs(",\"operations\":", file);
    writeArray(file, frame.operations, writeOperation);
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

    constexpr const char* kPoseNames[] = {"upper-left", "lower-left", "upper-right", "lower-right"};
    for (int lane = 0; lane < 4; ++lane) {
      startSelectedGame(core, kEggRow);
      handleEvent(core, tapDpad(lane));
      update(core);
      renderFresh(core);
      frames.push_back(captureFrame((std::string("nu-pogodi-") + kPoseNames[lane] + "-" + size.name).c_str()));
    }

    setEggPace(core, true, 100);
    for (int lane = 0; lane < 4; ++lane) {
      handleEvent(core, tapDpad(lane));
      update(core);
    }
    renderFresh(core);
    frames.push_back(captureFrame((std::string("nu-pogodi-midgame-") + size.name).c_str()));

    handleEvent(core, press(Button::Center));
    update(core);
    renderFresh(core);
    frames.push_back(captureFrame((std::string("nu-pogodi-paused-") + size.name).c_str()));

    startSelectedGame(core, 2);
    renderFresh(core);
    frames.push_back(captureFrame((std::string("falling-blocks-controls-") + size.name).c_str()));

    startSelectedGame(core, kSolitaireRow);
    renderFresh(core);
    frames.push_back(captureFrame((std::string("solitaire-initial-") + size.name).c_str()));

    handleEvent(core, tapSolitaireControl(0));
    update(core);
    renderFresh(core);
    frames.push_back(captureFrame((std::string("solitaire-stockdraw-") + size.name).c_str()));

    openSolitaireWinDialog(core);
    renderFresh(core);
    frames.push_back(captureFrame((std::string("completion-solitaire-win-") + size.name).c_str()));

    openEggGameOverDialog(core);
    renderFresh(core);
    frames.push_back(captureFrame((std::string("completion-egg-gameover-") + size.name).c_str()));

    if (size.width == 480 && size.height == 800) {
      load2048BoardForTest(core, won2048ForTest());
      renderFresh(core);
      frames.push_back(captureFrame("completion-2048-win-portrait"));

      openBlocksGameOverDialog(core);
      renderFresh(core);
      frames.push_back(captureFrame("completion-blocks-gameover-portrait"));
    }
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
    runner.expectTrue(hasCenteredText("Solitaire (Klondike)"), "chooser up wraps from first row to solitaire");
  }

  {
    startSelectedGame(core, kSolitaireRow);
    renderFresh(core);
    runner.expectTrue(hasCenteredText("Solitaire (Klondike)"),
                      "four downs then center launches solitaire from chooser");
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
    resetApp(core);
    handleEvent(core, tapChooserRow(4));
    renderFresh(core);
    runner.expectTrue(hasCenteredText("Solitaire (Klondike)"), "touching fifth chooser row launches solitaire");
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
    const auto noInputRects = fallingRectsAfterInput(core, Event::none());
    const auto buttonUpRects = fallingRectsAfterInput(core, press(Button::Up));
    const auto topEdgeRects = fallingRectsAfterInput(core, tapTopEdge());
    runner.expectTrue(rectsEqual(buttonUpRects, topEdgeRects), "falling blocks top edge matches up-button rotation");

    const auto buttonLeftRects = fallingRectsAfterInput(core, press(Button::Left));
    const auto leftPadRects = fallingRectsAfterInput(core, tapDpad(0));
    runner.expectFalse(rectsEqual(noInputRects, buttonLeftRects),
                       "falling blocks physical left changes active outline");
    runner.expectTrue(rectsEqual(buttonLeftRects, leftPadRects), "falling blocks left pad matches left button");

    const auto buttonDownRects = fallingRectsAfterInput(core, press(Button::Down));
    const auto downLeftHalfRects = fallingRectsAfterInput(core, tapDpad(1));
    const auto downRightHalfRects = fallingRectsAfterInput(core, tapDpad(2));
    runner.expectFalse(rectsEqual(noInputRects, buttonDownRects),
                       "falling blocks physical down changes active outline");
    runner.expectTrue(rectsEqual(buttonDownRects, downLeftHalfRects),
                      "falling blocks left half of down pad matches down button");
    runner.expectTrue(rectsEqual(buttonDownRects, downRightHalfRects),
                      "falling blocks right half of down pad matches down button");

    const auto buttonRightRects = fallingRectsAfterInput(core, press(Button::Right));
    const auto rightPadRects = fallingRectsAfterInput(core, tapDpad(3));
    runner.expectFalse(rectsEqual(noInputRects, buttonRightRects),
                       "falling blocks physical right changes active outline");
    runner.expectTrue(rectsEqual(buttonRightRects, rightPadRects), "falling blocks right pad matches right button");
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
    runner.expectTrue(handleEvent(core, tapDpad(0)), "playing touch dpad left is consumed");
  }

  {
    startSelectedGame(core, 2);
    runner.expectTrue(handleEvent(core, tapDpad(1)), "playing touch dpad down is consumed");
  }

  {
    startSelectedGame(core, 2);
    renderFresh(core);
    const auto controls = controlRects();
    runner.expectTrue(hasExactDrawnText("Left"), "generic game controls render left label");
    runner.expectTrue(hasExactDrawnText("Down"), "generic game controls render down label");
    runner.expectTrue(hasExactDrawnText("Right"), "generic game controls render right label");
    runner.expectFalse(hasExactDrawnText("Up"), "generic game controls do not render an up pad");
    runner.expectEq<size_t>(3, controls.size(), "generic game controls render three touch pads");
    runner.expectTrue(hasWideMiddleControl(controls), "generic game down control is visibly twice as wide");
  }

  for (const auto& size : kScreenSizes) {
    renderer.setScreenSize(size.width, size.height);
    startSelectedGame(core, 2);
    renderFresh(core);
    const auto controls = controlRects();
    runner.expectEq<size_t>(3, controls.size(), testName("generic control pad count", 2, size.name));
    runner.expectTrue(rectsStayOnScreen(controls), testName("generic control pads stay inside", 2, size.name));
    runner.expectTrue(hasWideMiddleControl(controls), testName("generic down pad is double-width", 2, size.name));
  }
  renderer.setScreenSize(480, 800);

  {
    startSelectedGame(core, 1);
    runner.expectTrue(handleEvent(core, tapCenterBoard()), "playing board center tap is consumed");
    runner.expectFalse(update(core), "playing board center tap does not trigger gameplay input");
  }

  {
    startSelectedGame(core, kSolitaireRow);
    testSetManualMillis(100000);
    runner.expectFalse(update(core), "solitaire does not advance on timer");
  }

  {
    startSelectedGame(core, kSolitaireRow);
    renderFresh(core);
    runner.expectTrue(hasExactDrawnText("Stock"), "solitaire renders stock control");
    runner.expectTrue(hasExactDrawnText("Undo"), "solitaire renders undo control");
    runner.expectTrue(hasExactDrawnText("Auto"), "solitaire renders auto control");
    runner.expectTrue(rectsStayOnScreen(renderer.drawRects()), "solitaire draw rects stay inside screen");
    runner.expectTrue(linesStayOnScreen(renderer.lineCalls()), "solitaire lines stay inside screen");
  }

  {
    startSelectedGame(core, kSolitaireRow);
    runner.expectTrue(handleEvent(core, tapSolitaireControl(0)), "solitaire stock touch is consumed");
    runner.expectTrue(update(core), "solitaire stock touch draws and redraws");
    runner.expectTrue(handleEvent(core, tapSolitaireControl(1)), "solitaire undo touch is consumed");
    runner.expectTrue(update(core), "solitaire undo touch redraws after stock draw");
    runner.expectTrue(handleEvent(core, tapOutsideScreen()), "solitaire outside touch is consumed");
    runner.expectFalse(update(core), "solitaire outside touch does not move cards");
  }

  {
    startSelectedGame(core, kSolitaireRow);
    const auto layout = solitaireLayoutForTest();
    runner.expectTrue(handleEvent(core, tap(-1, layout.controlsY + 10)), "solitaire rejects outside-x control-row tap");
    runner.expectFalse(update(core), "solitaire outside-x control-row tap does not redraw");
  }

  {
    Solitaire model;
    model.reset(100);
    int colWithHidden = 1;
    for (int col = 0; col < 7; ++col) {
      if (firstFaceUpForTest(model, Solitaire::TABLEAU_FIRST + col) > 0) {
        colWithHidden = col;
        break;
      }
    }
    const auto hiddenPrefix = solitaireColumnRectForTest(colWithHidden);
    startSelectedGame(core, kSolitaireRow);
    runner.expectTrue(handleEvent(core, tap(hiddenPrefix.x + hiddenPrefix.w / 2, hiddenPrefix.y + 10)),
                      "solitaire hidden tableau prefix tap is consumed");
    runner.expectFalse(update(core), "solitaire hidden tableau prefix tap does not select a face-up card");
  }

  {
    SolitaireMoveForTest move = {};
    if (findLegalSolitaireMove(move)) {
      Solitaire model;
      model.reset(100);
      startSelectedGame(core, kSolitaireRow);
      for (int i = 0; i < move.draws; ++i) {
        handleEvent(core, tapSolitaireControl(0));
        update(core);
        model.draw();
      }
      runner.expectTrue(handleEvent(core, tapPoint(solitaireCardPointForTest(model, move.fromPile, move.fromIndex))),
                        "solitaire legal source card tap is consumed");
      runner.expectTrue(update(core), "solitaire legal source selection redraws");
      runner.expectTrue(handleEvent(core, tapPoint(solitaireTargetPointForTest(move.toPile))),
                        "solitaire legal target tap is consumed");
      runner.expectTrue(update(core), "solitaire legal touch move redraws");
    } else {
      runner.expectTrue(true, "solitaire deterministic deal has no legal touch move to exercise");
    }
  }

  {
    Solitaire model;
    model.reset(100);
    const int pile = Solitaire::TABLEAU_FIRST;
    const int first = firstFaceUpForTest(model, pile);
    startSelectedGame(core, kSolitaireRow);
    runner.expectTrue(handleEvent(core, tapPoint(solitaireCardPointForTest(model, pile, first))),
                      "solitaire source selection before auto is consumed");
    runner.expectTrue(update(core), "solitaire source selection before auto redraws");
    runner.expectTrue(handleEvent(core, tapSolitaireControl(2)), "solitaire auto control after selection is consumed");
    update(core);
    runner.expectTrue(handleEvent(core, tapPoint(solitaireCardPointForTest(model, pile, first))),
                      "solitaire new selection after auto is consumed");
    runner.expectTrue(update(core), "solitaire new selection after auto has no stale selected source");
  }

  {
    Solitaire model;
    model.reset(100);
    const int pile = Solitaire::TABLEAU_FIRST;
    const int first = firstFaceUpForTest(model, pile);
    startSelectedGame(core, kSolitaireRow);
    runner.expectTrue(handleEvent(core, tapPoint(solitaireCardPointForTest(model, pile, first))),
                      "solitaire source selection before disabled undo is consumed");
    runner.expectTrue(update(core), "solitaire source selection before disabled undo redraws");
    runner.expectTrue(handleEvent(core, tapSolitaireControl(1)), "solitaire disabled undo after selection is consumed");
    runner.expectTrue(update(core), "solitaire disabled undo clears selection or redraws consistently");
    runner.expectTrue(handleEvent(core, tapPoint(solitaireCardPointForTest(model, pile, first))),
                      "solitaire can select source again after disabled undo");
    runner.expectTrue(update(core), "solitaire selection after disabled undo redraws");
  }

  {
    startSelectedGame(core, kSolitaireRow);
    runner.expectTrue(handleEvent(core, press(Button::Center)), "solitaire center draws from stock by default");
    runner.expectTrue(update(core), "solitaire center draw updates the view");
    runner.expectTrue(handleEvent(core, press(Button::Back)), "solitaire back opens pause menu");
    update(core);
    renderFresh(core);
    runner.expectTrue(hasCenteredText("Paused"), "solitaire back pauses instead of leaving chooser");
    runner.expectTrue(hasDrawnText("Undo"), "solitaire pause menu offers undo");
    runner.expectFalse(hasDrawnText("Pace:"), "solitaire pause menu has no pace option");
    handleEvent(core, press(Button::Down));
    handleEvent(core, press(Button::Down));
    handleEvent(core, press(Button::Center));
    runner.expectTrue(update(core), "solitaire pause undo row applies undo and resumes");
    renderFresh(core);
    runner.expectTrue(hasCenteredText("Solitaire (Klondike)"), "solitaire undo row returns to play screen");
  }

  {
    startSelectedGame(core, kSolitaireRow);
    runner.expectTrue(handleEvent(core, tapButtonBar(1)), "solitaire normal footer menu opens pause");
    update(core);
    renderFresh(core);
    runner.expectTrue(hasCenteredText("Paused"), "solitaire normal footer menu pauses");
  }

  {
    startSelectedGame(core, kSolitaireRow);
    runner.expectTrue(handleEvent(core, tapButtonBar(0)), "solitaire normal footer games is consumed");
    renderFresh(core);
    runner.expectTrue(hasCenteredText("Games"), "solitaire normal footer games returns to chooser");
  }

  {
    startSelectedGame(core, kSolitaireRow);
    core.settings.frontButtonLayout = Settings::FrontLRBC;
    runner.expectTrue(handleEvent(core, tapButtonBar(3)), "solitaire LRBC footer menu opens pause");
    update(core);
    renderFresh(core);
    runner.expectTrue(hasCenteredText("Paused"), "solitaire LRBC footer menu pauses");
  }

  {
    startSelectedGame(core, kSolitaireRow);
    core.settings.frontButtonLayout = Settings::FrontLRBC;
    runner.expectTrue(handleEvent(core, tapButtonBar(2)), "solitaire LRBC footer games is consumed");
    renderFresh(core);
    runner.expectTrue(hasCenteredText("Games"), "solitaire LRBC footer games returns to chooser");
  }

  {
    openSolitaireWinDialog(core);
    renderFresh(core);
    const auto dialog = finishDialogForTest();
    runner.expectTrue(hasCenteredText("You won!"), "solitaire win opens completion dialog");
    runner.expectTrue(hasDrawnText("Close"), "completion dialog renders Close action");
    runner.expectTrue(hasDrawnText("Play Again"), "completion dialog renders Play Again action");
    runner.expectTrue(hasRect(dialog.panel), "completion dialog renders expected panel rect");
    runner.expectTrue(hasRect(dialog.close), "completion dialog renders expected close button rect");
    runner.expectTrue(hasRect(dialog.again), "completion dialog renders expected play-again button rect");
    runner.expectTrue(renderer.getTextWidth(0, "Play Again") <= dialog.again.w,
                      "completion dialog play-again label fits button");
  }

  for (const auto& size : kScreenSizes) {
    renderer.setScreenSize(size.width, size.height);
    openSolitaireWinDialog(core);
    renderFresh(core);
    const auto dialog = finishDialogForTest();
    runner.expectTrue(testRectsStayOnScreen({dialog.panel, dialog.close, dialog.again}),
                      testName("completion dialog rects stay inside", kSolitaireRow, size.name));
    runner.expectTrue(hasRect(dialog.panel), testName("completion dialog panel rect", kSolitaireRow, size.name));
    runner.expectTrue(hasRect(dialog.close), testName("completion dialog close rect", kSolitaireRow, size.name));
    runner.expectTrue(hasRect(dialog.again), testName("completion dialog again rect", kSolitaireRow, size.name));
  }
  renderer.setScreenSize(480, 800);

  {
    openSolitaireWinDialog(core);
    runner.expectTrue(handleEvent(core, press(Button::Center)), "completion dialog defaults center to Play Again");
    update(core);
    renderFresh(core);
    runner.expectTrue(hasCenteredText("Moves: 0"), "completion Play Again starts current solitaire game over");
    runner.expectTrue(hasDrawnText("24"), "completion Play Again resets solitaire stock count");
  }

  {
    openSolitaireWinDialog(core);
    handleEvent(core, press(Button::Left));
    runner.expectTrue(handleEvent(core, press(Button::Center)), "completion left then center chooses Close");
    update(core);
    renderFresh(core);
    runner.expectTrue(hasCenteredText("Games"), "completion Close returns to chooser");
  }

  {
    openSolitaireWinDialog(core);
    runner.expectTrue(handleEvent(core, press(Button::Back)), "completion Back closes dialog");
    update(core);
    renderFresh(core);
    runner.expectTrue(hasCenteredText("Games"), "completion Back returns to chooser");
  }

  {
    openSolitaireWinDialog(core);
    const auto dialog = finishDialogForTest();
    runner.expectTrue(handleEvent(core, tapRectCenter(dialog.close)), "completion close tap is consumed");
    update(core);
    renderFresh(core);
    runner.expectTrue(hasCenteredText("Games"), "completion close tap returns to chooser");
  }

  {
    openSolitaireWinDialog(core);
    const auto dialog = finishDialogForTest();
    runner.expectTrue(handleEvent(core, tapRectCenter(dialog.again)), "completion play-again tap is consumed");
    update(core);
    renderFresh(core);
    runner.expectTrue(hasCenteredText("Moves: 0"), "completion play-again tap restarts current game");
  }

  {
    openSolitaireWinDialog(core);
    runner.expectTrue(handleEvent(core, tap(2, 2)), "completion outside tap is consumed");
    update(core);
    renderFresh(core);
    runner.expectTrue(hasCenteredText("You won!"), "completion outside tap leaves dialog open");
    runner.expectTrue(handleEvent(core, tapDpad(0)), "completion dpad-area tap is consumed");
    update(core);
    renderFresh(core);
    runner.expectTrue(hasCenteredText("You won!"), "completion dpad-area tap leaves dialog open");
  }

  {
    openSolitaireWinDialog(core);
    runner.expectTrue(handleEvent(core, tapButtonBar(0)), "completion normal footer games tap is consumed");
    update(core);
    renderFresh(core);
    runner.expectTrue(hasCenteredText("You won!"), "completion normal footer games does not dismiss");
    runner.expectTrue(handleEvent(core, tapButtonBar(1)), "completion normal footer menu tap is consumed");
    update(core);
    renderFresh(core);
    runner.expectTrue(hasCenteredText("You won!"), "completion normal footer menu does not dismiss");
  }

  {
    openSolitaireWinDialog(core);
    core.settings.frontButtonLayout = Settings::FrontLRBC;
    runner.expectTrue(handleEvent(core, tapButtonBar(2)), "completion LRBC footer games tap is consumed");
    update(core);
    renderFresh(core);
    runner.expectTrue(hasCenteredText("You won!"), "completion LRBC footer games does not dismiss");
    runner.expectTrue(handleEvent(core, tapButtonBar(3)), "completion LRBC footer menu tap is consumed");
    update(core);
    renderFresh(core);
    runner.expectTrue(hasCenteredText("You won!"), "completion LRBC footer menu does not dismiss");
  }

  {
    runner.expectTrue(openSnakeGameOverDialog(core), "snake terminal state opens completion dialog");
    runner.expectTrue(hasCenteredText("Game over"), "snake completion dialog title is game over");
  }

  {
    runner.expectTrue(openEggGameOverDialog(core), "Nu, Pogodi terminal misses open completion dialog");
    runner.expectTrue(hasCenteredText("Game over"), "Nu, Pogodi completion dialog title is game over");
    testSetManualMillis(1000000);
    runner.expectFalse(update(core), "completion dialog freezes timers");
  }

  {
    load2048BoardForTest(core, won2048ForTest());
    renderFresh(core);
    runner.expectTrue(hasCenteredText("You won!"), "2048 winning tile opens completion dialog");
    runner.expectTrue(handleEvent(core, press(Button::Center)), "2048 win completion defaults to Play Again");
    update(core);
    renderFresh(core);
    runner.expectFalse(hasCenteredText("You won!"), "2048 Play Again dismisses completion dialog");
    runner.expectTrue(hasCenteredText("Score: 0"), "2048 Play Again resets score");
  }

  {
    load2048BoardForTest(core, blocked2048ForTest());
    renderFresh(core);
    runner.expectTrue(hasCenteredText("Game over"), "2048 blocked checkerboard opens game-over dialog");
    handleEvent(core, press(Button::Left));
    runner.expectTrue(handleEvent(core, press(Button::Center)), "2048 game-over completion close is selected");
    update(core);
    renderFresh(core);
    runner.expectTrue(hasCenteredText("Games"), "2048 completion Close returns to chooser");
  }

  {
    runner.expectTrue(openBlocksGameOverDialog(core), "falling blocks terminal state opens completion dialog");
    runner.expectTrue(hasCenteredText("Game over"), "falling blocks completion dialog title is game over");
    runner.expectTrue(handleEvent(core, press(Button::Center)), "falling blocks completion defaults to Play Again");
    update(core);
    renderFresh(core);
    runner.expectTrue(hasCenteredText("Falling Blocks"), "falling blocks Play Again restarts current game");
    runner.expectFalse(hasCenteredText("Game over"), "falling blocks Play Again dismisses completion dialog");
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

#if PAPYRIX_NU_ORIGINAL_ART_AVAILABLE
  for (const auto& size : kScreenSizes) {
    const auto& artwork = papyrix::games::art::artworkFor(size.width, size.height);
    const int playWidth = size.width - 32;
    const int playHeight = size.height - 284;
    runner.expectTrue(artwork.width <= playWidth - 4,
                      testName("original Nu artwork width fits bordered play area", 3, size.name));
    runner.expectTrue(artwork.height <= playHeight - 4,
                      testName("original Nu artwork height fits bordered play area", 3, size.name));
    runner.expectTrue(aspectPreservedWithinOnePixel(artwork.width, artwork.height),
                      testName("original Nu artwork preserves source aspect", 3, size.name));

    expectInkAndBounds(runner, artwork, artwork.background, testName("original Nu background", 3, size.name));
    for (int body = 0; body < 2; ++body) {
      expectInkAndBounds(
          runner, artwork, artwork.bodies[body],
          testName(body == 0 ? "original Nu left-facing body" : "original Nu right-facing body", 3, size.name));
    }
    for (int basket = 0; basket < 4; ++basket) {
      expectInkAndBounds(runner, artwork, artwork.baskets[basket],
                         testName("original Nu basket mask", basket, size.name));
      for (int other = 0; other < basket; ++other) {
        const bool geometryDistinct = artwork.baskets[other].x != artwork.baskets[basket].x ||
                                      artwork.baskets[other].y != artwork.baskets[basket].y ||
                                      artwork.baskets[other].width != artwork.baskets[basket].width ||
                                      artwork.baskets[other].height != artwork.baskets[basket].height;
        runner.expectTrue(geometryDistinct, testName("original Nu basket geometry is distinct", basket, size.name));
        runner.expectNe(bitmapFingerprint(artwork.baskets[other]), bitmapFingerprint(artwork.baskets[basket]),
                        testName("original Nu basket masks are distinct", basket, size.name));
      }
    }
    for (int lane = 0; lane < 4; ++lane) {
      for (int position = 0; position < 5; ++position) {
        expectInkAndBounds(runner, artwork, artwork.eggs[lane][position],
                           testName("original Nu egg slot mask", lane * 5 + position, size.name));
      }
    }
  }

  {
    renderer.setScreenSize(480, 800);
    std::vector<std::vector<GfxRenderer::RectCall>> poseRects;
    for (int lane = 0; lane < 4; ++lane) {
      startSelectedGame(core, kEggRow);
      handleEvent(core, tapDpad(lane));
      update(core);
      renderFresh(core);
      poseRects.push_back(renderer.fillRects());
      runner.expectTrue(!renderer.fillRects().empty(),
                        testName("original Nu pose renders source masks", lane, "480x800"));
      runner.expectTrue(renderer.lineCalls().empty(),
                        testName("original Nu pose avoids fallback line art", lane, "480x800"));
    }
    runner.expectFalse(rectsEqual(poseRects[0], poseRects[1]), "original Nu upper-left and lower-left poses differ");
    runner.expectFalse(rectsEqual(poseRects[0], poseRects[2]), "original Nu left and right body poses differ");
    runner.expectFalse(rectsEqual(poseRects[2], poseRects[3]), "original Nu upper-right and lower-right poses differ");
  }
#endif

  {
    startSelectedGame(core, 1);
    renderFresh(core);
    runner.expectTrue(!renderer.fillRects().empty(), "snake render draws filled board cells");
    runner.expectTrue(rectsStayOnScreen(renderer.fillRects()), "snake filled cells stay inside screen bounds");
  }

  for (const auto& size : kScreenSizes) {
    renderer.setScreenSize(size.width, size.height);
    for (int gameRow = 0; gameRow < 5; ++gameRow) {
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
    std::printf("Wrote game screen frames to %s\n", screensPath);
  }
  testUseRealtimeMillis();
  return runner.allPassed() ? 0 : 1;
}
