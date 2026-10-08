#pragma once

#include <Display.h>
#include <EpdFontFamily.h>

#include <cstring>
#include <string>
#include <utility>
#include <vector>

class GfxRenderer {
 public:
  struct CenteredTextCall {
    int fontId;
    int y;
    std::string text;
    bool black;
    EpdFontFamily::Style style;
  };

  struct RectCall {
    int x;
    int y;
    int w;
    int h;
    bool color;
  };

  static constexpr int BUTTON_HINT_WIDTH = 106;
  static constexpr int BUTTON_HINT_MAX_TEXT_WIDTH = 94;

  explicit GfxRenderer(papyrix::hal::Display& display) : display_(display) {}

  void setScreenSize(int width, int height) {
    screenWidth_ = width;
    screenHeight_ = height;
  }

  int getTextWidth(int, const char* text, EpdFontFamily::Style = EpdFontFamily::REGULAR) const {
    return text ? static_cast<int>(std::strlen(text)) * 8 : 0;
  }

  int getLineHeight(int) const { return 20; }
  int getScreenWidth() const { return screenWidth_; }
  int getScreenHeight() const { return screenHeight_; }

  const std::vector<CenteredTextCall>& centeredTextCalls() const { return centeredTextCalls_; }
  void clearCenteredTextCalls() const { centeredTextCalls_.clear(); }
  const std::vector<RectCall>& drawRects() const { return drawRects_; }
  const std::vector<RectCall>& fillRects() const { return fillRects_; }
  void clearDrawRects() const { drawRects_.clear(); }
  void clearFillRects() const { fillRects_.clear(); }
  void clearRects() const {
    drawRects_.clear();
    fillRects_.clear();
  }
  const std::string& lastText() const { return lastText_; }

  void setWrappedTextResult(std::vector<std::string> lines) { wrappedTextResult_ = std::move(lines); }
  std::vector<std::string> wrapTextWithHyphenation(int, const char* text, int, int maxLines,
                                                   EpdFontFamily::Style = EpdFontFamily::REGULAR) const {
    if (!wrappedTextResult_.empty()) return wrappedTextResult_;
    if (!text || !*text || maxLines <= 0) return {};
    return {text};
  }

  std::string truncatedText(int, const char* text, int) const { return text ? text : ""; }
  void clearScreen(uint8_t color = 0xFF) const { display_.clearScreen(color); }
  void clearArea(int, int, int, int, uint8_t = 0xFF) const {}
  void drawLine(int, int, int, int, bool = true) const {}
  void drawRect(int x, int y, int w, int h, bool color = true) const { drawRects_.push_back({x, y, w, h, color}); }
  void fillRect(int x, int y, int w, int h, bool color = true) const { fillRects_.push_back({x, y, w, h, color}); }
  void drawText(int, int, int, const char* text, bool = true, EpdFontFamily::Style = EpdFontFamily::REGULAR) const {
    lastText_ = text ? text : "";
  }
  void drawCenteredText(int fontId, int y, const char* text, bool black = true,
                        EpdFontFamily::Style style = EpdFontFamily::REGULAR) const {
    centeredTextCalls_.push_back({fontId, y, text ? text : "", black, style});
  }
  void drawImage(const uint8_t*, int, int, int, int) const {}
  void displayBuffer(papyrix::hal::Display::RefreshMode = papyrix::hal::Display::FAST_REFRESH, bool = false) const {}

 private:
  papyrix::hal::Display& display_;
  int screenWidth_ = 480;
  int screenHeight_ = 800;
  mutable std::vector<CenteredTextCall> centeredTextCalls_;
  mutable std::vector<RectCall> drawRects_;
  mutable std::vector<RectCall> fillRects_;
  mutable std::string lastText_;
  std::vector<std::string> wrappedTextResult_;
};
