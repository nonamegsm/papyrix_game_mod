#include "QrCodeApp.h"

#include <GfxRenderer.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "../core/Core.h"
#include "../ui/Elements.h"
#include "QrCodeStore.h"
#include "QrSymbol.h"
#include "ThemeManager.h"

extern GfxRenderer renderer;

namespace papyrix::qr_app {
namespace {
constexpr int LIST_Y = 80;
constexpr int ROW_HEIGHT = 44;
enum class Screen : uint8_t { List, Display, Error };
Screen screen = Screen::List;
codes::QrCodeStore store;
codes::QrCodeInfo entries[codes::MAX_QR_CODES]{};
codes::QrCodeEntry active{};
codes::QrSymbol symbol;
size_t count = 0;
size_t selected = 0;
bool dirty = true;
char error[96]{};

int pageRows() { return std::max(1, (renderer.getScreenHeight() - LIST_Y - 100) / ROW_HEIGHT); }
size_t pageStart() { return (selected / pageRows()) * pageRows(); }

void reload() {
  count = 0;
  const auto result = store.list(entries, codes::MAX_QR_CODES, count);
  if (result != codes::StoreResult::Ok) {
    snprintf(error, sizeof(error), "%s", codes::storeErrorMessage(result));
    screen = Screen::Error;
  } else {
    selected = count ? std::min(selected, count - 1) : 0;
    screen = Screen::List;
    error[0] = '\0';
  }
  dirty = true;
}

void openSelected() {
  if (!count) return;
  const auto result = store.load(entries[selected].id, active);
  if (result != codes::StoreResult::Ok) {
    snprintf(error, sizeof(error), "%s", codes::storeErrorMessage(result));
    screen = Screen::Error;
  } else if (!codes::encodeQrCode(active.data, symbol)) {
    snprintf(error, sizeof(error), "Could not encode this QR code");
    screen = Screen::Error;
  } else {
    screen = Screen::Display;
  }
  dirty = true;
}

void move(int delta) {
  if (!count) return;
  selected =
      static_cast<size_t>((static_cast<int>(selected) + delta + static_cast<int>(count)) % static_cast<int>(count));
  if (screen == Screen::Display) openSelected();
  dirty = true;
}

bool button(Button btn) {
  if (btn == Button::Back) {
    if (screen == Screen::Error) {
      reload();
      return true;
    }
    return false;
  }
  if (screen == Screen::Error) {
    if (btn == Button::Center) reload();
    return true;
  }
  if (screen == Screen::Display) {
    if (btn == Button::Center) {
      screen = Screen::List;
      dirty = true;
    } else if (btn == Button::Left || btn == Button::Up)
      move(-1);
    else if (btn == Button::Right || btn == Button::Down)
      move(1);
    return true;
  }
  if (btn == Button::Up)
    move(-1);
  else if (btn == Button::Down)
    move(1);
  else if (btn == Button::Center)
    openSelected();
  else if (btn == Button::Left || btn == Button::Right) {
    if (count) {
      const size_t rows = static_cast<size_t>(pageRows());
      const size_t pageCount = (count + rows - 1) / rows;
      const size_t page = selected / rows;
      selected = ((page + (btn == Button::Left ? pageCount - 1 : 1)) % pageCount) * rows;
      dirty = true;
    }
  }
  return true;
}
}  // namespace

void enter(Core&) {
  selected = 0;
  reload();
}
void exit(Core&) {}
bool update(Core&) {
  const bool changed = dirty;
  dirty = false;
  return changed;
}

bool handleEvent(Core& core, const Event& event) {
  if (event.type == EventType::ButtonPress) return button(event.button);
  if (event.type == EventType::ButtonRepeat) return true;
  if (event.type != EventType::Tap) return false;
  const ui::touch::Point point{event.touch.x, event.touch.y};
  const int w = renderer.getScreenWidth(), h = renderer.getScreenHeight();
  const int action =
      ui::touch::semanticButtonBarIndex(point, w, h, core.settings.frontButtonLayout == Settings::FrontLRBC);
  if (action >= 0) {
    constexpr Button buttons[] = {Button::Back, Button::Center, Button::Left, Button::Right};
    return button(buttons[action]);
  }
  if (screen == Screen::List && count) {
    const size_t first = pageStart();
    const int visible = static_cast<int>(std::min(count - first, static_cast<size_t>(pageRows())));
    const int row = ui::touch::rowAt(
        point, {0, LIST_Y, static_cast<int16_t>(w), static_cast<int16_t>(visible * ROW_HEIGHT)}, ROW_HEIGHT, visible);
    if (row >= 0) {
      selected = first + row;
      openSelected();
    }
  }
  return true;
}

bool render(Core&) {
  renderer.clearScreen(THEME.backgroundColor);
  ui::title(renderer, THEME, THEME.screenMarginTop, "QR Codes");
  if (screen == Screen::Display && symbol.valid) {
    const auto label = renderer.truncatedText(THEME.uiFontId, active.name, renderer.getScreenWidth() - 32);
    renderer.drawCenteredText(THEME.uiFontId, 52, label.c_str(), THEME.primaryTextBlack);
    const auto rect = codes::qrCodeRect(renderer.getScreenWidth(), renderer.getScreenHeight(), symbol.code.size);
    if (rect.modulePixels) {
      renderer.fillRect(rect.x, rect.y, rect.width, rect.height, false);
      for (uint8_t y = 0; y < symbol.code.size; ++y) {
        for (uint8_t x = 0; x < symbol.code.size; ++x) {
          if (symbol.module(x, y))
            renderer.fillRect(rect.x + (x + 4) * rect.modulePixels, rect.y + (y + 4) * rect.modulePixels,
                              rect.modulePixels, rect.modulePixels, true);
        }
      }
    }
    ui::buttonBar(renderer, THEME, ui::ButtonBar("Back", "List", count > 1 ? "<" : "", count > 1 ? ">" : ""));
  } else if (screen == Screen::Error) {
    const auto line = renderer.truncatedText(THEME.uiFontId, error, renderer.getScreenWidth() - 32);
    renderer.drawCenteredText(THEME.uiFontId, 100, line.c_str(), THEME.primaryTextBlack);
    ui::buttonBar(renderer, THEME, ui::ButtonBar("Back", "Retry", "", ""));
  } else if (!count) {
    renderer.drawCenteredText(THEME.uiFontId, 100, "No saved QR codes", THEME.primaryTextBlack);
    renderer.drawCenteredText(THEME.smallFontId, 150, "Open Apps > WiFi Transfer", THEME.primaryTextBlack);
    renderer.drawCenteredText(THEME.smallFontId, 182, "Connect and open the web page", THEME.primaryTextBlack);
    renderer.drawCenteredText(THEME.smallFontId, 214, "Save a code in the QR Codes tab", THEME.primaryTextBlack);
    renderer.drawCenteredText(THEME.smallFontId, 246, "Then leave WiFi and open this app", THEME.primaryTextBlack);
    ui::buttonBar(renderer, THEME, ui::ButtonBar("Back", "", "", ""));
  } else {
    const size_t first = pageStart();
    const size_t end = std::min(count, first + pageRows());
    for (size_t i = first; i < end; ++i) {
      const auto label = renderer.truncatedText(THEME.uiFontId, entries[i].name,
                                                renderer.getScreenWidth() - 2 * THEME.screenMarginSide - 24);
      ui::menuItem(renderer, THEME, LIST_Y + static_cast<int>(i - first) * ROW_HEIGHT, label.c_str(), i == selected);
    }
    char page[40];
    snprintf(page, sizeof(page), "Page %u of %u", static_cast<unsigned>(first / pageRows() + 1),
             static_cast<unsigned>((count + pageRows() - 1) / pageRows()));
    renderer.drawCenteredText(THEME.smallFontId, renderer.getScreenHeight() - 82, page, THEME.primaryTextBlack);
    ui::buttonBar(renderer, THEME, ui::ButtonBar("Back", "Open", "<", ">"));
  }
  renderer.displayBuffer(hal::Display::FULL_REFRESH, true);
  return true;
}
}  // namespace papyrix::qr_app
