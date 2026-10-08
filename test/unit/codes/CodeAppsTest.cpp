#include <Arduino.h>
#include <Display.h>
#include <GfxRenderer.h>
#include <SDCardManager.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <ostream>
#include <string>
#include <vector>

#include "apps/BarcodeApp.h"
#include "apps/QrCodeApp.h"
#include "apps/QrCodeStore.h"
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

constexpr const char* kBarcodePath = "/.papyrix/apps/barcode.txt";
constexpr const char* kUtf8QrName =
    "Bilet \xC5\x81\xC3\xB3"
    "d\xC5\xBA";
constexpr int kFormatRowY = 80;
constexpr int kRowHeight = 44;

struct ScreenSize {
  int width;
  int height;
  const char* name;
};

constexpr ScreenSize kScreenSizes[] = {
    {480, 800, "portrait"},
    {800, 480, "landscape"},
    {528, 792, "x3 portrait"},
    {792, 528, "x3 landscape"},
};

Event press(Button button) { return Event::buttonPress(button); }

Event tap(int x, int y) { return Event::tap({static_cast<int16_t>(x), static_cast<int16_t>(y)}); }

Event tapButtonBar(int visualIndex) {
  const auto rect = ui::touch::buttonBarButtonRect(visualIndex, renderer.getScreenWidth(), renderer.getScreenHeight());
  return tap(rect.x + rect.width / 2, rect.y + rect.height / 2);
}

Event tapKeyboardKey(int row, int column) {
  constexpr int keyboardY = 110;
  constexpr int borderPadding = 10;
  constexpr int keySpacing = 2;
  constexpr int keyHeight = 20;
  constexpr int rowPitch = 26;
  const int screenMarginSide = 3;
  const int gridWidth = renderer.getScreenWidth() - 2 * screenMarginSide - 2 * borderPadding;
  const int keyWidth = (gridWidth - 9 * keySpacing) / 10;
  const int separators = (row > 0 ? 1 : 0) + (row > 3 ? 1 : 0) + (row > 6 ? 1 : 0);
  const int x = screenMarginSide + borderPadding + column * (keyWidth + keySpacing) + keyWidth / 2;
  const int y = keyboardY + 10 + row * rowPitch + separators * 18 + keyHeight / 2;
  return tap(x, y);
}

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

bool rectsStayOnScreen(const std::vector<GfxRenderer::RectCall>& rects) {
  for (const auto& rect : rects) {
    if (rect.w <= 0 || rect.h <= 0) return false;
    if (rect.x < 0 || rect.y < 0) return false;
    if (rect.x + rect.w > renderer.getScreenWidth()) return false;
    if (rect.y + rect.h > renderer.getScreenHeight()) return false;
  }
  return true;
}

std::string named(const char* prefix, const char* suffix) {
  std::string value(prefix);
  value += " ";
  value += suffix;
  return value;
}

void renderBarcode(Core& core) {
  renderer.clearCalls();
  papyrix::barcode_app::render(core);
}

void renderQr(Core& core) {
  renderer.clearCalls();
  papyrix::qr_app::render(core);
}

void resetBarcode(Core& core) {
  SdMan.reset();
  core.settings = Settings{};
  renderer.setScreenSize(480, 800);
  renderer.clearCalls();
  papyrix::barcode_app::enter(core);
  papyrix::barcode_app::update(core);
}

void resetQr(Core& core) {
  core.settings = Settings{};
  renderer.clearCalls();
  papyrix::qr_app::enter(core);
  papyrix::qr_app::update(core);
}

void typeCode128AB(Core& core) {
  papyrix::barcode_app::handleEvent(core, tapKeyboardKey(4, 0));
  papyrix::barcode_app::handleEvent(core, tapKeyboardKey(4, 1));
}

void typeDigits(Core& core, const char* digits) {
  for (const char* p = digits; *p; ++p) {
    const int column = *p == '0' ? 9 : *p - '1';
    papyrix::barcode_app::handleEvent(core, tapKeyboardKey(7, column));
  }
}

void confirmKeyboard(Core& core) { papyrix::barcode_app::handleEvent(core, tapKeyboardKey(0, 8)); }

void startCode128Editing(Core& core) { papyrix::barcode_app::handleEvent(core, tap(20, kFormatRowY + 8)); }

void startEan13Editing(Core& core) { papyrix::barcode_app::handleEvent(core, tap(20, kFormatRowY + kRowHeight + 8)); }

void saveCode128AB(Core& core) {
  resetBarcode(core);
  startCode128Editing(core);
  typeCode128AB(core);
  confirmKeyboard(core);
  papyrix::barcode_app::update(core);
}

void fillQrStore() {
  SdMan.reset();
  papyrix::codes::QrCodeStore store;
  uint8_t id = 99;
  for (int i = 0; i < 16; ++i) {
    char name[48] = {};
    char data[64] = {};
    std::snprintf(name, sizeof(name), "QR %02d", i);
    std::snprintf(data, sizeof(data), "payload-%02d", i);
    store.save(-1, name, data, id);
  }
}

void seedUtf8QrStore() {
  SdMan.reset();
  papyrix::codes::QrCodeStore store;
  uint8_t id = 99;
  store.save(-1, kUtf8QrName, "linia pierwsza\nemoji-like plain text", id);
  store.save(-1, "Second", "second-payload", id);
}

void seedDemoQrStore() {
  SdMan.reset();
  papyrix::codes::QrCodeStore store;
  uint8_t id = 99;
  store.save(-1, "Papyrix Game Mod", "https://github.com/nonamegsm/papyrix_game_mod", id);
}

struct CapturedFrame {
  std::string name;
  int screenWidth = 0;
  int screenHeight = 0;
  uint8_t backgroundColor = 0xFF;
  std::vector<GfxRenderer::RectCall> fillRects;
  std::vector<GfxRenderer::RectCall> drawRects;
  std::vector<GfxRenderer::TextCall> textCalls;
  std::vector<GfxRenderer::CenteredTextCall> centeredTextCalls;
};

CapturedFrame captureFrame(const char* name) {
  return {name,
          renderer.getScreenWidth(),
          renderer.getScreenHeight(),
          renderer.clearColor(),
          renderer.fillRects(),
          renderer.drawRects(),
          renderer.textCalls(),
          renderer.centeredTextCalls()};
}

void writeJsonEscaped(std::ostream& out, const std::string& value) {
  out << '"';
  for (unsigned char c : value) {
    switch (c) {
      case '\\':
        out << "\\\\";
        break;
      case '"':
        out << "\\\"";
        break;
      case '\n':
        out << "\\n";
        break;
      case '\r':
        out << "\\r";
        break;
      case '\t':
        out << "\\t";
        break;
      default:
        if (c < 0x20) {
          constexpr char kHex[] = "0123456789abcdef";
          out << "\\u00" << kHex[(c >> 4) & 0x0F] << kHex[c & 0x0F];
        } else {
          out << static_cast<char>(c);
        }
        break;
    }
  }
  out << '"';
}

void writeRectJson(std::ostream& out, const GfxRenderer::RectCall& rect) {
  out << "{\"x\":" << rect.x << ",\"y\":" << rect.y << ",\"w\":" << rect.w << ",\"h\":" << rect.h
      << ",\"black\":" << (rect.color ? "true" : "false") << "}";
}

void writeTextJson(std::ostream& out, const GfxRenderer::TextCall& call) {
  out << "{\"fontId\":" << call.fontId << ",\"x\":" << call.x << ",\"y\":" << call.y
      << ",\"black\":" << (call.black ? "true" : "false") << ",\"style\":" << static_cast<int>(call.style)
      << ",\"text\":";
  writeJsonEscaped(out, call.text);
  out << "}";
}

void writeCenteredTextJson(std::ostream& out, const GfxRenderer::CenteredTextCall& call) {
  out << "{\"fontId\":" << call.fontId << ",\"y\":" << call.y << ",\"black\":" << (call.black ? "true" : "false")
      << ",\"style\":" << static_cast<int>(call.style) << ",\"text\":";
  writeJsonEscaped(out, call.text);
  out << "}";
}

template <typename T, typename Writer>
void writeArray(std::ostream& out, const std::vector<T>& values, Writer writer) {
  out << '[';
  for (size_t i = 0; i < values.size(); ++i) {
    if (i) out << ',';
    writer(out, values[i]);
  }
  out << ']';
}

void writeFrameJson(std::ostream& out, const CapturedFrame& frame) {
  out << "{\"name\":";
  writeJsonEscaped(out, frame.name);
  out << ",\"screenWidth\":" << frame.screenWidth << ",\"screenHeight\":" << frame.screenHeight
      << ",\"backgroundColor\":" << static_cast<unsigned>(frame.backgroundColor) << ",\"fillRects\":";
  writeArray(out, frame.fillRects, writeRectJson);
  out << ",\"drawRects\":";
  writeArray(out, frame.drawRects, writeRectJson);
  out << ",\"text\":";
  writeArray(out, frame.textCalls, writeTextJson);
  out << ",\"centeredText\":";
  writeArray(out, frame.centeredTextCalls, writeCenteredTextJson);
  out << "}";
}

void writeFramesJson(const std::vector<CapturedFrame>& frames, const std::filesystem::path& outputPath) {
  const auto parent = outputPath.parent_path();
  if (!parent.empty()) std::filesystem::create_directories(parent);
  std::ofstream out(outputPath, std::ios::binary | std::ios::trunc);
  out << "{\"frames\":[";
  for (size_t i = 0; i < frames.size(); ++i) {
    if (i) out << ',';
    writeFrameJson(out, frames[i]);
  }
  out << "]}\n";
}

void exportScreens(Core& core, const std::filesystem::path& outputPath) {
  std::vector<CapturedFrame> frames;

  resetBarcode(core);
  renderBarcode(core);
  frames.push_back(captureFrame("barcode-format-chooser"));

  resetBarcode(core);
  startCode128Editing(core);
  typeCode128AB(core);
  renderBarcode(core);
  frames.push_back(captureFrame("barcode-keyboard-code128"));

  saveCode128AB(core);
  renderBarcode(core);
  frames.push_back(captureFrame("barcode-code128-display"));

  resetBarcode(core);
  startEan13Editing(core);
  typeDigits(core, "400638133393");
  confirmKeyboard(core);
  renderBarcode(core);
  frames.push_back(captureFrame("barcode-ean13-display"));

  seedDemoQrStore();
  renderer.setScreenSize(480, 800);
  resetQr(core);
  papyrix::qr_app::handleEvent(core, press(Button::Center));
  renderQr(core);
  frames.push_back(captureFrame("qr-display-demo-url"));

  writeFramesJson(frames, outputPath);
}

std::filesystem::path screenOutputPath(int argc, char** argv) {
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i] ? argv[i] : "";
    if (arg == "--screens") {
      if (i + 1 < argc && argv[i + 1] && argv[i + 1][0] != '-') return argv[i + 1];
      return ".omx/codes-preview/frames.json";
    }
    constexpr const char* kPrefix = "--screens=";
    if (arg.rfind(kPrefix, 0) == 0) return arg.substr(std::strlen(kPrefix));
  }
  return {};
}

}  // namespace

int main(int argc, char** argv) {
  TestUtils::TestRunner runner("CodeAppsTest");
  Core core;

  {
    resetBarcode(core);
    renderBarcode(core);
    runner.expectTrue(hasCenteredText("Barcode"), "barcode app starts on format chooser when no saved code exists");
    runner.expectTrue(hasDrawnText("Code 128 - letters and numbers"), "format chooser offers Code 128");
    runner.expectTrue(hasDrawnText("EAN-13 - product barcode"), "format chooser offers EAN-13");
  }

  {
    resetBarcode(core);
    core.settings.frontButtonLayout = Settings::FrontLRBC;
    runner.expectFalse(papyrix::barcode_app::handleEvent(core, tapButtonBar(2)),
                       "barcode LRBC visual back button exits empty chooser");
  }

  {
    resetBarcode(core);
    startEan13Editing(core);
    typeDigits(core, "4006381333932");
    confirmKeyboard(core);
    renderBarcode(core);
    runner.expectTrue(hasDrawnText("barcode checksum is invalid"),
                      "invalid EAN-13 checksum is shown inline on keyboard");
    runner.expectFalse(SdMan.exists(kBarcodePath), "invalid EAN-13 does not publish a barcode file");
  }

  {
    resetBarcode(core);
    startCode128Editing(core);
    papyrix::barcode_app::handleEvent(core, tapKeyboardKey(4, 0));
    papyrix::barcode_app::handleEvent(core, tapKeyboardKey(4, 1));
    papyrix::barcode_app::handleEvent(core, tapKeyboardKey(0, 1));
    confirmKeyboard(core);
    renderBarcode(core);
    runner.expectTrue(hasCenteredText("Code 128"), "keyboard backspace removes the last typed character before save");
    runner.expectTrue(hasCenteredText("A"), "saved Code 128 label reflects edited keyboard input");
    runner.expectEqual("B\nA", SdMan.getWrittenData(kBarcodePath),
                       "Code 128 save writes format header and edited data");
  }

  {
    saveCode128AB(core);
    renderBarcode(core);
    runner.expectTrue(hasCenteredText("Code 128"), "valid Code 128 save opens display screen");
    runner.expectTrue(hasCenteredText("AB"), "valid Code 128 render labels the encoded data");
    runner.expectEqual("B\nAB", SdMan.getWrittenData(kBarcodePath), "valid Code 128 persists as last barcode");

    const auto& rects = renderer.fillRects();
    const auto white = std::find_if(rects.begin(), rects.end(), [](const GfxRenderer::RectCall& rect) {
      return !rect.color && rect.w > 100 && rect.h > 50;
    });
    const auto black = std::find_if(rects.begin(), rects.end(), [](const GfxRenderer::RectCall& rect) {
      return rect.color && rect.w > 0 && rect.h > 50;
    });
    runner.expectTrue(white != rects.end(), "Code 128 render paints a white quiet-zone background");
    runner.expectTrue(black != rects.end(), "Code 128 render paints black bars on top of the white background");
    if (white != rects.end() && black != rects.end()) {
      runner.expectTrue(black->x > white->x, "Code 128 first black bar starts after left quiet zone");
      runner.expectTrue(black->x + black->w <= white->x + white->w, "Code 128 bars stay inside quiet-zone background");
    }
  }

  {
    saveCode128AB(core);
    papyrix::barcode_app::handleEvent(core, press(Button::Center));
    papyrix::barcode_app::handleEvent(core, press(Button::Back));
    renderBarcode(core);
    runner.expectTrue(hasCenteredText("AB"), "back while editing an existing barcode cancels and returns to display");
  }

  {
    SdMan.reset();
    SdMan.registerFile("/.papyrix/apps/barcode.bak", "B\nBACKUP");
    renderer.clearCalls();
    papyrix::barcode_app::enter(core);
    papyrix::barcode_app::update(core);
    renderBarcode(core);
    runner.expectTrue(hasCenteredText("BACKUP"),
                      "barcode app restores last code from backup when final file is absent");
  }

  {
    seedUtf8QrStore();
    renderer.setScreenSize(480, 800);
    resetQr(core);
    renderQr(core);
    runner.expectTrue(hasCenteredText("QR Codes"), "QR app opens saved-list screen");
    runner.expectTrue(hasDrawnText(kUtf8QrName), "QR list renders UTF-8 metadata names");
    papyrix::qr_app::handleEvent(core, press(Button::Center));
    renderQr(core);
    runner.expectTrue(hasCenteredText(kUtf8QrName), "QR display renders selected UTF-8 name");
    runner.expectTrue(!renderer.fillRects().empty(), "QR display draws encoded modules from persisted store data");
  }

  {
    seedUtf8QrStore();
    resetQr(core);
    papyrix::qr_app::handleEvent(core, press(Button::Center));
    papyrix::qr_app::exit(core);
    resetQr(core);
    renderQr(core);
    runner.expectTrue(hasDrawnText(kUtf8QrName), "QR app restart reloads persisted records");
  }

  {
    fillQrStore();
    renderer.setScreenSize(800, 480);
    resetQr(core);
    papyrix::qr_app::handleEvent(core, press(Button::Right));
    papyrix::qr_app::handleEvent(core, press(Button::Right));
    renderQr(core);
    runner.expectTrue(hasDrawnText("QR 12"), "QR list right paging reaches the first item on page three");
    runner.expectTrue(hasCenteredText("Page 3 of 3"), "QR list shows page count for 16 landscape entries");
    papyrix::qr_app::handleEvent(core, press(Button::Center));
    renderQr(core);
    runner.expectTrue(hasCenteredText("QR 12"), "QR list opens the selected paged entry");
    papyrix::qr_app::handleEvent(core, press(Button::Right));
    renderQr(core);
    runner.expectTrue(hasCenteredText("QR 13"), "QR display next advances to the next stored entry");
    papyrix::qr_app::handleEvent(core, press(Button::Left));
    renderQr(core);
    runner.expectTrue(hasCenteredText("QR 12"), "QR display previous returns to the prior stored entry");
    papyrix::qr_app::handleEvent(core, press(Button::Center));
    renderQr(core);
    runner.expectTrue(hasDrawnText("QR 12"), "QR display center returns to the list with selection preserved");
  }

  {
    seedUtf8QrStore();
    for (const auto& size : kScreenSizes) {
      renderer.setScreenSize(size.width, size.height);
      resetQr(core);
      papyrix::qr_app::handleEvent(core, press(Button::Center));
      renderQr(core);
      runner.expectTrue(rectsStayOnScreen(renderer.drawRects()), named("QR draw rects stay inside", size.name));
      runner.expectTrue(rectsStayOnScreen(renderer.fillRects()), named("QR fill rects stay inside", size.name));
    }
  }

  runner.printSummary();
  const auto screensPath = screenOutputPath(argc, argv);
  if (!screensPath.empty()) {
    exportScreens(core, screensPath);
    std::cout << "Wrote screen command frames to " << screensPath.string() << "\n";
  }
  return runner.allPassed() ? 0 : 1;
}
