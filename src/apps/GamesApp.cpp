#include "GamesApp.h"

#include <Arduino.h>
#include <GfxRenderer.h>

#include <algorithm>
#include <cstdio>

#include "../core/Core.h"
#include "../ui/Elements.h"
#include "GameModels.h"
#include "GameTouchMargins.h"
#include "NuPogodiArtwork.h"
#include "ThemeManager.h"

extern GfxRenderer renderer;

namespace papyrix::games_app {
namespace {

enum class Screen : uint8_t { Chooser, Playing, Paused };
enum class Game : uint8_t { Tiles, Snake, Blocks, Eggs };
constexpr const char* NAMES[] = {"2048", "Snake", "Falling Blocks", "Nu, Pogodi!"};
constexpr int GAME_COUNT = sizeof(NAMES) / sizeof(NAMES[0]);
constexpr const char* BASKET_NAMES[] = {"Upper left", "Lower left", "Upper right", "Lower right"};
constexpr int LIST_Y = 80;
constexpr int ROW_HEIGHT = 44;
constexpr int CONTROL_HEIGHT = 72;
constexpr int FOOTER_HEIGHT = 50;

games::Game2048 tiles;
games::Snake snake;
games::FallingBlocks blocks;
games::EggCatcher eggs;
Screen screen = Screen::Chooser;
Game game = Game::Tiles;
int selected = 0;
int pauseSelected = 0;
bool dirty = true;
bool fullRefresh = true;
bool manual = false;
uint32_t lastStep = 0;
uint8_t refreshCount = 0;

bool ended() {
  switch (game) {
    case Game::Tiles:
      return tiles.gameOver();
    case Game::Snake:
      return snake.gameOver() || snake.won();
    case Game::Blocks:
      return blocks.gameOver();
    case Game::Eggs:
      return eggs.gameOver();
  }
  return true;
}

void start() {
  game = static_cast<Game>(selected);
  const uint32_t seed = millis();
  switch (game) {
    case Game::Tiles:
      tiles.reset(seed);
      break;
    case Game::Snake:
      snake.reset(seed);
      break;
    case Game::Blocks:
      blocks.reset(seed);
      break;
    case Game::Eggs:
      eggs.reset(seed);
      break;
  }
  screen = Screen::Playing;
  lastStep = millis();
  fullRefresh = dirty = true;
}

void pauseAction() {
  switch (pauseSelected) {
    case 0:
      screen = Screen::Playing;
      lastStep = millis();
      break;
    case 1:
      start();
      break;
    case 2:
      if (game == Game::Tiles)
        screen = Screen::Chooser;
      else
        manual = !manual;
      break;
    case 3:
      screen = Screen::Chooser;
      break;
  }
  dirty = true;
}

void selectBasket(int lane) {
  if (ended()) return;
  dirty |= eggs.selectLane(lane);
  if (manual) dirty |= eggs.step();
}

void playButton(Button button) {
  if (button == Button::Back) {
    screen = Screen::Chooser;
    dirty = true;
    return;
  }
  if (button == Button::Center) {
    screen = Screen::Paused;
    pauseSelected = ended() ? 1 : 0;
    dirty = true;
    return;
  }
  if (ended()) return;
  games::Direction direction;
  switch (button) {
    case Button::Up:
      direction = games::Direction::Up;
      break;
    case Button::Down:
      direction = games::Direction::Down;
      break;
    case Button::Left:
      direction = games::Direction::Left;
      break;
    case Button::Right:
      direction = games::Direction::Right;
      break;
    default:
      return;
  }
  switch (game) {
    case Game::Tiles:
      dirty |= tiles.move(direction);
      break;
    case Game::Snake:
      if (snake.turn(direction) && manual) dirty |= snake.step();
      break;
    case Game::Blocks:
      switch (button) {
        case Button::Left:
          dirty |= blocks.move(-1);
          break;
        case Button::Right:
          dirty |= blocks.move(1);
          break;
        case Button::Up:
          dirty |= blocks.rotate();
          break;
        case Button::Down:
          dirty |= blocks.step();
          break;
        default:
          break;
      }
      break;
    case Game::Eggs: {
      int lane = eggs.basketLane();
      if (button == Button::Up) lane &= 2;
      if (button == Button::Down) lane |= 1;
      if (button == Button::Left) lane &= 1;
      if (button == Button::Right) lane |= 2;
      selectBasket(lane);
      break;
    }
  }
}

void button(Button btn) {
  if (screen == Screen::Playing) {
    playButton(btn);
    return;
  }
  int& item = screen == Screen::Chooser ? selected : pauseSelected;
  const int count = screen == Screen::Chooser ? GAME_COUNT : game == Game::Tiles ? 3 : 4;
  if (btn == Button::Up || btn == Button::Left) {
    item = (item + count - 1) % count;
    dirty = true;
  } else if (btn == Button::Down || btn == Button::Right) {
    item = (item + 1) % count;
    dirty = true;
  } else if (btn == Button::Center) {
    if (screen == Screen::Chooser)
      start();
    else
      pauseAction();
  } else if (btn == Button::Back && screen == Screen::Paused) {
    screen = Screen::Playing;
    lastStep = millis();
    dirty = true;
  }
}

void centered(int y, const char* text, int font = -1) {
  renderer.drawCenteredText(font < 0 ? THEME.uiFontId : font, y, text, THEME.primaryTextBlack);
}

struct BoardLayout {
  int x, y, cell;
};
BoardLayout boardLayout(int columns, int rows) {
  const int cell =
      std::max(1, std::min((renderer.getScreenWidth() - 32) / columns, (renderer.getScreenHeight() - 260) / rows));
  return {(renderer.getScreenWidth() - columns * cell) / 2, 90, cell};
}

ui::touch::Rect eggPlayArea() {
  return {16, 112, static_cast<int16_t>(renderer.getScreenWidth() - 32),
          static_cast<int16_t>(renderer.getScreenHeight() - 284)};
}

#if PAPYRIX_NU_ORIGINAL_ART_AVAILABLE
// Transparent monochrome masks retain the original SVG outlines and positions.
void drawArt(const games::art::Bitmap& image, int originX, int originY) {
  const int rowBytes = (image.width + 7) / 8;
  for (int y = 0; y < image.height; ++y) {
    int start = -1;
    for (int x = 0; x < image.width; ++x) {
      const bool ink = (image.data[y * rowBytes + x / 8] & (0x80 >> (x & 7))) == 0;
      if (ink && start < 0) start = x;
      if (!ink && start >= 0) {
        renderer.fillRect(originX + image.x + start, originY + image.y + y, x - start, 1, THEME.primaryTextBlack);
        start = -1;
      }
    }
    if (start >= 0) {
      renderer.fillRect(originX + image.x + start, originY + image.y + y, image.width - start, 1,
                        THEME.primaryTextBlack);
    }
  }
}

void renderEggs() {
  const auto area = eggPlayArea();
  const auto& art = games::art::artworkFor(renderer.getScreenWidth(), renderer.getScreenHeight());
  const int x = area.x + (area.width - art.width) / 2;
  const int y = area.y + (area.height - art.height) / 2;
  renderer.drawRect(x - 2, y - 2, art.width + 4, art.height + 4, THEME.primaryTextBlack);
  const int lane = eggs.basketLane();
  drawArt(art.background, x, y);
  drawArt(art.bodies[lane >= 2 ? 1 : 0], x, y);
  drawArt(art.baskets[lane], x, y);
  drawArt(art.rabbit[0], x, y);
  drawArt(art.rabbit[1], x, y);
  drawArt(art.gameB, x, y);
  // Segment order: top, upper right, lower right, bottom, lower left, upper left, middle.
  constexpr uint8_t DIGITS[] = {0x3f, 0x06, 0x5b, 0x4f, 0x66, 0x6d, 0x7d, 0x07, 0x7f, 0x6f};
  uint32_t score = eggs.score() % 1000;
  for (int digit = 2; digit >= 0; --digit) {
    const uint8_t segments = DIGITS[score % 10];
    score /= 10;
    for (int bit = 0; bit < 7; ++bit) {
      if (segments & (1u << bit)) drawArt(art.digits[digit][bit], x, y);
    }
  }
  for (int miss = 0; miss < eggs.misses(); ++miss) drawArt(art.misses[miss], x, y);
  for (int channel = 0; channel < games::EggCatcher::LANES; ++channel) {
    for (int position = 0; position < games::EggCatcher::POSITIONS; ++position) {
      if (eggs.egg(channel, position)) {
        // Keep the original five LCD egg positions; the catch window holds the last one.
        drawArt(art.eggs[channel][std::min(position, 4)], x, y);
      }
    }
  }
  char text[48];
  snprintf(text, sizeof(text), "Basket: %s", BASKET_NAMES[lane]);
  centered(82, text, THEME.smallFontId);
}

#else
// Small original pixel drawings, scaled to the available play area.
void pixelSprite(const uint16_t* rows, int count, int x, int y, int scale, bool mirror) {
  for (int row = 0; row < count; ++row) {
    for (int col = 0; col < 16;) {
      if (!(rows[row] & (0x8000u >> col))) {
        ++col;
        continue;
      }
      const int first = col;
      while (col < 16 && (rows[row] & (0x8000u >> col))) ++col;
      const int sx = mirror ? 16 - col : first;
      renderer.fillRect(x + sx * scale, y + row * scale, (col - first) * scale, scale, THEME.primaryTextBlack);
    }
  }
}

void renderEggs() {
  constexpr uint16_t WOLF[] = {0x1800, 0x3c00, 0x3e00, 0x7f00, 0xff80, 0xbff0, 0x7ffe, 0x3ffc,
                               0x0ff0, 0x0e00, 0x3f00, 0x7f80, 0x7f80, 0x4200, 0x7f80, 0x7f80,
                               0x4200, 0x7f80, 0xffc0, 0xfb80, 0x6380, 0x6380, 0x6300, 0x7780};
  constexpr uint16_t HEN[] = {0x0600, 0x0f00, 0x1d80, 0x1fe0, 0x0fc0, 0x3f00,
                              0x7f80, 0xff80, 0x7f00, 0x3e00, 0x1400, 0x3600};
  constexpr uint16_t EGG[] = {0x1800, 0x2400, 0x4200, 0x8100, 0x8100, 0x8100, 0x8100, 0x4200, 0x3c00};
  const auto area = eggPlayArea();
  const int scale = std::max(2, std::min(area.width / 80, area.height / 60));
  const int cx = renderer.getScreenWidth() / 2;
  const int wolfY = area.y + area.height / 2 - 12 * scale;
  const bool ink = THEME.primaryTextBlack;
  const bool right = eggs.basketLane() >= 2;
  renderer.drawRect(area.x, area.y, area.width, area.height, ink);
  pixelSprite(WOLF, 24, cx - 8 * scale, wolfY, scale, !right);
  for (int lane = 0; lane < games::EggCatcher::LANES; ++lane) {
    const bool onRight = lane >= 2;
    const int targetX = cx + (onRight ? 1 : -1) * area.width / 5;
    const int sourceX = area.x + (onRight ? 7 : 1) * area.width / 8;
    const int targetY = area.y + ((lane & 1) ? 2 : 1) * area.height / 3;
    const int sourceY = targetY - area.height / 9;
    pixelSprite(HEN, 12, sourceX - 8 * scale, sourceY - 13 * scale, scale, onRight);
    renderer.drawLine(sourceX, sourceY + 8, targetX, targetY + 8, ink);
    renderer.drawLine(sourceX, sourceY + 12, targetX, targetY + 12, ink);
    for (int pos = 0; pos < games::EggCatcher::POSITIONS; ++pos) {
      if (!eggs.egg(lane, pos)) continue;
      const int x = sourceX + (targetX - sourceX) * pos / (games::EggCatcher::POSITIONS - 1);
      const int y = sourceY + (targetY - sourceY) * pos / (games::EggCatcher::POSITIONS - 1);
      const int eggScale = std::max(1, scale / 2);
      pixelSprite(EGG, 9, x - 4 * eggScale, y - 8 * eggScale, eggScale, false);
    }
    const int basketY = targetY + 16;
    renderer.drawRect(targetX - 4 * scale, basketY, 8 * scale, 4 * scale, ink);
    if (lane == eggs.basketLane()) {
      renderer.fillRect(targetX - 4 * scale, basketY + 2 * scale, 8 * scale, 2 * scale, ink);
      renderer.drawLine(targetX - 4 * scale, basketY, targetX, basketY - 3 * scale, ink);
      renderer.drawLine(targetX, basketY - 3 * scale, targetX + 4 * scale, basketY, ink);
      renderer.drawLine(cx + (onRight ? 3 : -3) * scale, wolfY + 12 * scale, targetX, basketY, ink);
    }
  }
  char text[48];
  snprintf(text, sizeof(text), "Basket: %s", BASKET_NAMES[eggs.basketLane()]);
  centered(82, text, THEME.smallFontId);
}

#endif

void renderBoard() {
  char text[64];
  const uint32_t score = game == Game::Tiles    ? tiles.score()
                         : game == Game::Snake  ? snake.score()
                         : game == Game::Blocks ? blocks.score()
                                                : eggs.score();
  if (game == Game::Blocks) {
    snprintf(text, sizeof(text), "Score: %lu   Lines: %lu", static_cast<unsigned long>(score),
             static_cast<unsigned long>(blocks.lines()));
  } else if (game == Game::Eggs) {
    snprintf(text, sizeof(text), "Score: %lu   Misses: %u/%d", static_cast<unsigned long>(score),
             static_cast<unsigned>(eggs.misses()), games::EggCatcher::MAX_MISSES);
  } else {
    snprintf(text, sizeof(text), "Score: %lu", static_cast<unsigned long>(score));
  }
  centered(52, text);
  const bool ink = THEME.primaryTextBlack;
  if (game == Game::Eggs) {
    renderEggs();
  } else if (game == Game::Tiles) {
    const auto b = boardLayout(4, 4);
    for (int row = 0; row < 4; ++row) {
      for (int col = 0; col < 4; ++col) {
        const int x = b.x + col * b.cell, y = b.y + row * b.cell;
        renderer.drawRect(x, y, b.cell - 3, b.cell - 3, ink);
        const auto value = tiles.tile(row, col);
        if (!value) continue;
        snprintf(text, sizeof(text), "%lu", static_cast<unsigned long>(value));
        int font = THEME.readerFontId;
        if (renderer.getTextWidth(font, text) > b.cell - 10) font = THEME.uiFontId;
        if (renderer.getTextWidth(font, text) > b.cell - 10) font = THEME.smallFontId;
        const int tx = x + (b.cell - 3 - renderer.getTextWidth(font, text)) / 2;
        const int ty = y + (b.cell - 3 - renderer.getLineHeight(font)) / 2;
        renderer.drawText(font, tx, ty, text, ink);
      }
    }
  } else {
    const int columns = game == Game::Snake ? games::Snake::WIDTH : games::FallingBlocks::WIDTH;
    const int rows = game == Game::Snake ? games::Snake::HEIGHT : games::FallingBlocks::HEIGHT;
    const auto b = boardLayout(columns, rows);
    renderer.drawRect(b.x - 2, b.y - 2, columns * b.cell + 4, rows * b.cell + 4, ink);
    for (int y = 0; y < rows; ++y) {
      for (int x = 0; x < columns; ++x) {
        const int px = b.x + x * b.cell, py = b.y + y * b.cell;
        if (game == Game::Snake) {
          if (snake.body(x, y))
            renderer.fillRect(px + 1, py + 1, b.cell - 2, b.cell - 2, ink);
          else if (x == snake.foodX() && y == snake.foodY())
            renderer.drawRect(px + 1, py + 1, b.cell - 2, b.cell - 2, ink);
        } else if (blocks.occupied(x, y)) {
          if (blocks.settled(x, y))
            renderer.fillRect(px + 1, py + 1, b.cell - 2, b.cell - 2, ink);
          else
            renderer.drawRect(px + 1, py + 1, b.cell - 2, b.cell - 2, ink);
        }
      }
    }
  }
  const int h = renderer.getScreenHeight(), w = renderer.getScreenWidth();
  const char* message = game == Game::Snake && snake.won()   ? "Board complete!"
                        : ended()                            ? "Game over - Menu to restart"
                        : game == Game::Tiles && tiles.won() ? "2048 reached! Keep playing"
                        : game == Game::Blocks               ? "Top: rotate   Down: lower"
                        : game == Game::Eggs
                            ? (manual ? "Basket tap = one step" : "Catch eggs - three misses end the game")
                        : game == Game::Snake ? (manual ? "Direction = one step" : "Slow pace - Menu to pause")
                                              : "Merge tiles to reach 2048";
  centered(h - 162, message, THEME.smallFontId);
  centered(h - 140, game == Game::Eggs ? "Tap a chute or use the screen edges" : "Tap top/bottom/left/right edges",
           THEME.smallFontId);
  constexpr const char* controls[] = {"Left", "Down", "Right"};
  const int count = game == Game::Eggs ? 4 : 3;
  for (int i = 0; i < count; ++i) {
    const char* label = game == Game::Eggs ? BASKET_NAMES[i] : controls[i];
    const int font = game == Game::Eggs ? THEME.smallFontId : THEME.uiFontId;
    const int firstQuarter = game != Game::Eggs && i == 2 ? 3 : i;
    const int quarters = game != Game::Eggs && i == 1 ? 2 : 1;
    const int x = firstQuarter * w / 4, width = (firstQuarter + quarters) * w / 4 - x;
    const int y = h - FOOTER_HEIGHT - CONTROL_HEIGHT;
    renderer.drawRect(x + 3, y + 4, width - 6, CONTROL_HEIGHT - 8, ink);
    renderer.drawText(font, x + (width - renderer.getTextWidth(font, label)) / 2,
                      y + (CONTROL_HEIGHT - renderer.getLineHeight(font)) / 2, label, ink);
  }
  ui::buttonBar(renderer, THEME, ui::ButtonBar("Games", "Menu", "<", ">"));
}

}  // namespace

void enter(Core&) {
  screen = Screen::Chooser;
  selected = pauseSelected = 0;
  dirty = fullRefresh = true;
  refreshCount = 0;
}

void exit(Core&) { screen = Screen::Chooser; }

bool handleEvent(Core& core, const Event& event) {
  if (event.type == EventType::ButtonRepeat) return true;
  if (event.type == EventType::ButtonPress) {
    if (event.button == Button::Back && screen == Screen::Chooser) return false;
    button(event.button);
    return true;
  }
  if (event.type != EventType::Tap) return false;
  const auto p = ui::touch::Point{event.touch.x, event.touch.y};
  const int w = renderer.getScreenWidth(), h = renderer.getScreenHeight();
  const int action = ui::touch::semanticButtonBarIndex(p, w, h, core.settings.frontButtonLayout == Settings::FrontLRBC);
  if (action >= 0) {
    constexpr Button buttons[] = {Button::Back, Button::Center, Button::Left, Button::Right};
    if (action == 0 && screen == Screen::Chooser) return false;
    button(buttons[action]);
  } else if (screen == Screen::Playing) {
    const int index = ui::touch::gridIndexAt(
        p, {0, static_cast<int16_t>(h - FOOTER_HEIGHT - CONTROL_HEIGHT), static_cast<int16_t>(w), CONTROL_HEIGHT}, 4,
        1);
    if (index >= 0) {
      if (game == Game::Eggs) {
        selectBasket(index);
      } else {
        constexpr Button buttons[] = {Button::Left, Button::Down, Button::Down, Button::Right};
        button(buttons[index]);
      }
    } else {
      const Button direction = touchMarginDirection(p, w, h);
      if (direction != Button::Count) {
        button(direction);
      } else if (game == Game::Eggs) {
        const int quadrant = ui::touch::gridIndexAt(p, eggPlayArea(), 2, 2);
        if (quadrant >= 0) selectBasket((quadrant % 2) * 2 + quadrant / 2);
      }
    }
  } else {
    const int count = screen == Screen::Chooser ? GAME_COUNT : game == Game::Tiles ? 3 : 4;
    const int row = ui::touch::rowAt(p, {0, LIST_Y, static_cast<int16_t>(w), static_cast<int16_t>(count * ROW_HEIGHT)},
                                     ROW_HEIGHT, count);
    if (row >= 0) {
      if (screen == Screen::Chooser) {
        selected = row;
        start();
      } else {
        pauseSelected = row;
        pauseAction();
      }
    }
  }
  return true;
}

bool update(Core&) {
  if (screen == Screen::Playing && game != Game::Tiles && !manual && !ended()) {
    const uint32_t now = millis();
    const uint32_t interval = game == Game::Snake ? 900 : game == Game::Eggs ? eggs.stepIntervalMs() : 1500;
    if (now - lastStep >= interval) {
      lastStep = now;
      dirty |= game == Game::Snake ? snake.step() : game == Game::Eggs ? eggs.step() : blocks.step();
    }
  }
  const bool changed = dirty;
  dirty = false;
  return changed;
}

bool render(Core&) {
  const uint32_t renderStarted = millis();
  renderer.clearScreen(THEME.backgroundColor);
  ui::title(renderer, THEME, THEME.screenMarginTop, screen == Screen::Chooser ? "Games" : NAMES[selected]);
  if (screen == Screen::Playing) {
    renderBoard();
  } else {
    if (screen == Screen::Paused) centered(48, ended() ? "Game over" : "Paused");
    const char* menu[] = {"Resume", "New game",
                          game == Game::Tiles ? "Choose game"
                          : manual            ? "Pace: Turn-based"
                                              : "Pace: Slow",
                          "Choose game"};
    const int count = screen == Screen::Chooser ? GAME_COUNT : game == Game::Tiles ? 3 : 4;
    for (int i = 0; i < count; ++i) {
      ui::menuItem(renderer, THEME, LIST_Y + i * ROW_HEIGHT, screen == Screen::Chooser ? NAMES[i] : menu[i],
                   i == (screen == Screen::Chooser ? selected : pauseSelected));
    }
    if (screen == Screen::Chooser) {
      centered(renderer.getScreenHeight() - 120, "Puzzles and slow-paced games for e-paper", THEME.smallFontId);
    }
    ui::buttonBar(renderer, THEME, ui::ButtonBar("Back", screen == Screen::Chooser ? "Play" : "Select", "<", ">"));
  }
  if (++refreshCount >= 40) fullRefresh = true;
  renderer.displayBuffer(fullRefresh ? hal::Display::FULL_REFRESH : hal::Display::FAST_REFRESH, true);
  if (fullRefresh) refreshCount = 0;
  fullRefresh = false;
  // Pause the egg timer only for panel work; moving the basket must not reset it.
  // The other games begin each interval after rendering, as before.
  if (game == Game::Eggs && screen == Screen::Playing)
    lastStep += millis() - renderStarted;
  else
    lastStep = millis();
  return true;
}

}  // namespace papyrix::games_app
