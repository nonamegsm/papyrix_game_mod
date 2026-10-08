#include "GamesApp.h"

#include <Arduino.h>
#include <GfxRenderer.h>

#include <algorithm>
#include <cstdio>

#include "../core/Core.h"
#include "../ui/Elements.h"
#include "GameModels.h"
#include "ThemeManager.h"

extern GfxRenderer renderer;

namespace papyrix::games_app {
namespace {

enum class Screen : uint8_t { Chooser, Playing, Paused };
enum class Game : uint8_t { Tiles, Snake, Blocks };
constexpr const char* NAMES[] = {"2048", "Snake", "Falling Blocks"};
constexpr int LIST_Y = 80;
constexpr int ROW_HEIGHT = 44;
constexpr int CONTROL_HEIGHT = 72;
constexpr int FOOTER_HEIGHT = 50;

games::Game2048 tiles;
games::Snake snake;
games::FallingBlocks blocks;
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
  }
}

void button(Button btn) {
  if (screen == Screen::Playing) {
    playButton(btn);
    return;
  }
  int& item = screen == Screen::Chooser ? selected : pauseSelected;
  const int count = screen == Screen::Chooser || game == Game::Tiles ? 3 : 4;
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

void renderBoard() {
  char text[64];
  const uint32_t score = game == Game::Tiles ? tiles.score() : game == Game::Snake ? snake.score() : blocks.score();
  if (game == Game::Blocks) {
    snprintf(text, sizeof(text), "Score: %lu   Lines: %lu", static_cast<unsigned long>(score),
             static_cast<unsigned long>(blocks.lines()));
  } else {
    snprintf(text, sizeof(text), "Score: %lu", static_cast<unsigned long>(score));
  }
  centered(52, text);
  const bool ink = THEME.primaryTextBlack;
  if (game == Game::Tiles) {
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
                        : game == Game::Blocks               ? "Up: rotate   Down: lower"
                        : game == Game::Snake ? (manual ? "Direction = one step" : "Slow pace - Menu to pause")
                                              : "Merge tiles to reach 2048";
  centered(h - 162, message, THEME.smallFontId);
  constexpr const char* controls[] = {"Up", "Down", "Left", "Right"};
  for (int i = 0; i < 4; ++i) {
    const int x = i * w / 4, y = h - FOOTER_HEIGHT - CONTROL_HEIGHT;
    renderer.drawRect(x + 3, y + 4, w / 4 - 6, CONTROL_HEIGHT - 8, ink);
    renderer.drawText(THEME.uiFontId, x + (w / 4 - renderer.getTextWidth(THEME.uiFontId, controls[i])) / 2,
                      y + (CONTROL_HEIGHT - renderer.getLineHeight(THEME.uiFontId)) / 2, controls[i], ink);
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
      constexpr Button buttons[] = {Button::Up, Button::Down, Button::Left, Button::Right};
      button(buttons[index]);
    }
  } else {
    const int count = screen == Screen::Chooser || game == Game::Tiles ? 3 : 4;
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
    const uint32_t interval = game == Game::Snake ? 900 : 1500;
    if (now - lastStep >= interval) {
      lastStep = now;
      dirty |= game == Game::Snake ? snake.step() : blocks.step();
    }
  }
  const bool changed = dirty;
  dirty = false;
  return changed;
}

bool render(Core&) {
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
    const int count = screen == Screen::Chooser || game == Game::Tiles ? 3 : 4;
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
  // Begin the next interval after the panel finishes; never catch up missed ticks.
  lastStep = millis();
  return true;
}

}  // namespace papyrix::games_app
