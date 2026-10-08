#include "BarcodeApp.h"

#include <GfxRenderer.h>
#include <SDCardManager.h>
#include <SdFat.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "../core/Core.h"
#include "../ui/Elements.h"
#include "../ui/views/UtilityViews.h"
#include "BarcodeEncoder.h"
#include "QrSymbol.h"
#include "ThemeManager.h"

extern GfxRenderer renderer;

namespace papyrix::barcode_app {
namespace {
constexpr const char* PATH = "/.papyrix/apps/barcode.txt";
constexpr const char* TMP_PATH = "/.papyrix/apps/barcode.tmp";
constexpr const char* BACKUP_PATH = "/.papyrix/apps/barcode.bak";
constexpr int LIST_Y = 80;
constexpr int ROW_HEIGHT = 44;
enum class Screen : uint8_t { Formats, Keyboard, Display };
Screen screen = Screen::Formats;
codes::BarcodeFormat format = codes::BarcodeFormat::Code128;
codes::BarcodeSymbol symbol;
codes::BarcodeSymbol candidate;
ui::KeyboardView keyboard;
bool dirty = true;
bool haveBarcode = false;
char error[96]{};

const char* formatName(codes::BarcodeFormat value) {
  return value == codes::BarcodeFormat::Ean13 ? "EAN-13" : "Code 128";
}

bool saveBarcode() {
  if (!SdMan.ready()) return false;
  if (!SdMan.exists("/.papyrix/apps") && !SdMan.mkdir("/.papyrix/apps")) return false;
  auto file = SdMan.open(TMP_PATH, O_WRONLY | O_CREAT | O_TRUNC);
  if (!file) return false;
  const char header[] = {candidate.format == codes::BarcodeFormat::Ean13 ? 'E' : 'B', '\n'};
  const size_t len = strlen(candidate.data);
  const bool written =
      file.write(header, sizeof(header)) == sizeof(header) && file.write(candidate.data, len) == len && file.sync();
  file.close();
  if (!written) {
    SdMan.remove(TMP_PATH);
    return false;
  }
  if (SdMan.exists(PATH)) {
    if (SdMan.exists(BACKUP_PATH) && !SdMan.remove(BACKUP_PATH)) return false;
    if (!SdMan.rename(PATH, BACKUP_PATH)) return false;
  }
  if (!SdMan.rename(TMP_PATH, PATH)) {
    if (SdMan.exists(BACKUP_PATH)) SdMan.rename(BACKUP_PATH, PATH);
    return false;
  }
  SdMan.remove(BACKUP_PATH);
  return true;
}

bool loadBarcode() {
  for (const char* path : {PATH, BACKUP_PATH}) {
    auto file = SdMan.open(path, O_RDONLY);
    if (!file) continue;
    char saved[codes::MAX_BARCODE_DATA_BYTES + 3]{};
    const size_t size = file.size();
    if (size < 3 || size >= sizeof(saved) || file.read(saved, size) != static_cast<int>(size)) {
      file.close();
      continue;
    }
    file.close();
    if (saved[1] != '\n' || (saved[0] != 'B' && saved[0] != 'E') || strlen(saved) != size) continue;
    const auto kind = saved[0] == 'E' ? codes::BarcodeFormat::Ean13 : codes::BarcodeFormat::Code128;
    if (codes::encodeBarcode(kind, saved + 2, symbol) == codes::BarcodeError::None) {
      format = kind;
      return true;
    }
  }
  return false;
}

void startEditing() {
  keyboard.clear();
  keyboard.keyboard = ui::KeyboardState{};
  keyboard.setPassword(false);
  keyboard.statusText = nullptr;
  keyboard.setTitle(format == codes::BarcodeFormat::Ean13 ? "EAN-13: 12 or 13 digits" : "Code 128: enter data");
  if (haveBarcode && symbol.format == format) {
    for (const char* p = symbol.data; *p; ++p) keyboard.appendChar(*p);
  }
  if (format == codes::BarcodeFormat::Ean13) keyboard.keyboard.cursorY = 7;
  error[0] = '\0';
  screen = Screen::Keyboard;
  dirty = true;
}

void confirm() {
  const auto result = codes::encodeBarcode(format, keyboard.input, candidate);
  if (result != codes::BarcodeError::None) {
    snprintf(error, sizeof(error), "%s", codes::barcodeErrorMessage(result));
  } else if (!codes::barcodeRect(renderer.getScreenWidth(), renderer.getScreenHeight(), candidate.moduleCount)
                  .modulePixels) {
    snprintf(error, sizeof(error), "Too wide: shorten data or use landscape");
  } else if (!saveBarcode()) {
    snprintf(error, sizeof(error), "Could not save to SD card. Try again.");
  } else {
    symbol = candidate;
    haveBarcode = true;
    screen = Screen::Display;
    error[0] = '\0';
  }
  dirty = true;
}

bool button(Button btn) {
  if (btn == Button::Back) {
    if (screen == Screen::Keyboard && haveBarcode) {
      format = symbol.format;
      screen = Screen::Display;
      dirty = true;
      return true;
    }
    if (screen == Screen::Keyboard) {
      screen = Screen::Formats;
      dirty = true;
      return true;
    }
    if (screen == Screen::Formats && haveBarcode) {
      screen = Screen::Display;
      dirty = true;
      return true;
    }
    return false;
  }
  if (screen == Screen::Formats) {
    if (btn == Button::Up || btn == Button::Down || btn == Button::Left || btn == Button::Right) {
      format = format == codes::BarcodeFormat::Code128 ? codes::BarcodeFormat::Ean13 : codes::BarcodeFormat::Code128;
      dirty = true;
    } else if (btn == Button::Center)
      startEditing();
  } else if (screen == Screen::Display) {
    if (btn == Button::Center) {
      format = symbol.format;
      startEditing();
    } else if (btn == Button::Left || btn == Button::Right) {
      screen = Screen::Formats;
      dirty = true;
    }
  } else {
    switch (btn) {
      case Button::Up:
        keyboard.moveUp();
        break;
      case Button::Down:
        keyboard.moveDown();
        break;
      case Button::Left:
        keyboard.moveLeft();
        break;
      case Button::Right:
        keyboard.moveRight();
        break;
      case Button::Center:
        if (keyboard.confirmKey()) confirm();
        break;
      default:
        return true;
    }
    dirty = true;
  }
  return true;
}
}  // namespace

void enter(Core&) {
  error[0] = '\0';
  format = codes::BarcodeFormat::Code128;
  haveBarcode = loadBarcode();
  screen = haveBarcode ? Screen::Display : Screen::Formats;
  dirty = true;
}
void exit(Core&) { keyboard.clear(); }
bool update(Core&) {
  const bool changed = dirty;
  dirty = false;
  return changed;
}

bool handleEvent(Core& core, const Event& event) {
  if (event.type == EventType::ButtonPress) return button(event.button);
  if (event.type == EventType::ButtonRepeat) {
    if (screen == Screen::Keyboard && (event.button == Button::Up || event.button == Button::Down ||
                                       event.button == Button::Left || event.button == Button::Right))
      button(event.button);
    return true;
  }
  if (event.type != EventType::Tap) return false;
  const ui::touch::Point point{event.touch.x, event.touch.y};
  const int w = renderer.getScreenWidth(), h = renderer.getScreenHeight();
  const bool lrbc = core.settings.frontButtonLayout == Settings::FrontLRBC;
  const int action = ui::touch::semanticButtonBarIndex(point, w, h, lrbc);
  if (action >= 0) {
    constexpr Button buttons[] = {Button::Back, Button::Center, Button::Left, Button::Right};
    return button(buttons[action]);
  }
  if (screen == Screen::Keyboard) {
    const auto hit = keyboard.hitTest(point, w, h, THEME.screenMarginSide, lrbc);
    if (hit.type == ui::KeyboardView::Hit::Type::Key) {
      keyboard.keyboard.cursorY = static_cast<int8_t>(hit.row);
      keyboard.keyboard.cursorX = static_cast<int8_t>(hit.column);
      button(Button::Center);
    }
  } else if (screen == Screen::Formats) {
    const int row = ui::touch::rowAt(point, {0, LIST_Y, static_cast<int16_t>(w), 2 * ROW_HEIGHT}, ROW_HEIGHT, 2);
    if (row >= 0) {
      format = row ? codes::BarcodeFormat::Ean13 : codes::BarcodeFormat::Code128;
      startEditing();
    }
  }
  return true;
}

bool render(Core&) {
  if (screen == Screen::Keyboard) {
    keyboard.statusText = error[0] ? error : nullptr;
    ui::render(renderer, THEME, keyboard);
    return true;
  }
  renderer.clearScreen(THEME.backgroundColor);
  ui::title(renderer, THEME, THEME.screenMarginTop, "Barcode");
  if (screen == Screen::Formats) {
    ui::menuItem(renderer, THEME, LIST_Y, "Code 128 - letters and numbers", format == codes::BarcodeFormat::Code128);
    ui::menuItem(renderer, THEME, LIST_Y + ROW_HEIGHT, "EAN-13 - product barcode",
                 format == codes::BarcodeFormat::Ean13);
    renderer.drawCenteredText(THEME.smallFontId, renderer.getScreenHeight() - 110,
                              "Choose a format, then enter its data", THEME.primaryTextBlack);
    ui::buttonBar(renderer, THEME, ui::ButtonBar("Back", "Enter", "<", ">"));
  } else {
    renderer.drawCenteredText(THEME.uiFontId, 52, formatName(symbol.format), THEME.primaryTextBlack);
    const auto rect = codes::barcodeRect(renderer.getScreenWidth(), renderer.getScreenHeight(), symbol.moduleCount);
    if (rect.modulePixels) {
      renderer.fillRect(rect.x, rect.y - 8, rect.width, rect.height + 16, false);
      for (size_t i = 0; i < symbol.moduleCount; ++i) {
        if (symbol.modules[i])
          renderer.fillRect(rect.x + static_cast<int>(i) * rect.modulePixels, rect.y, rect.modulePixels, rect.height,
                            true);
      }
      const auto label = renderer.truncatedText(THEME.uiFontId, symbol.data, renderer.getScreenWidth() - 32);
      renderer.drawCenteredText(THEME.uiFontId, rect.y + rect.height + 28, label.c_str(), THEME.primaryTextBlack);
    } else {
      renderer.drawCenteredText(THEME.uiFontId, 110, "Too wide for this orientation", THEME.primaryTextBlack);
      renderer.drawCenteredText(THEME.smallFontId, 145, "Use landscape or edit the data", THEME.primaryTextBlack);
    }
    ui::buttonBar(renderer, THEME, ui::ButtonBar("Back", "Edit", "Format", "Format"));
  }
  renderer.displayBuffer(hal::Display::FULL_REFRESH, true);
  return true;
}
}  // namespace papyrix::barcode_app
