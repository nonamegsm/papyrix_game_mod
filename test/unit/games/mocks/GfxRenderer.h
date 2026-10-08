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

  struct TextCall {
    int fontId;
    int x;
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

  struct LineCall {
    int x0;
    int y0;
    int x1;
    int y1;
    bool color;
  };

  struct DrawCall {
    enum class Kind { Rect, Fill, Line, Text, Centered } kind;
    int fontId = 0;
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
    int x1 = 0;
    int y1 = 0;
    std::string text;
    bool black = true;
    EpdFontFamily::Style style = EpdFontFamily::REGULAR;
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
  const std::vector<TextCall>& textCalls() const { return textCalls_; }
  void clearTextCalls() const {
    textCalls_.clear();
    lastText_.clear();
  }
  const std::vector<RectCall>& drawRects() const { return drawRects_; }
  const std::vector<RectCall>& fillRects() const { return fillRects_; }
  const std::vector<LineCall>& lineCalls() const { return lineCalls_; }
  const std::vector<DrawCall>& operations() const { return operations_; }
  uint8_t clearColor() const { return clearColor_; }
  void clearDrawRects() const { drawRects_.clear(); }
  void clearFillRects() const { fillRects_.clear(); }
  void clearLineCalls() const { lineCalls_.clear(); }
  void clearOperations() const { operations_.clear(); }
  void clearRects() const {
    drawRects_.clear();
    fillRects_.clear();
  }
  void clearCalls() const {
    clearCenteredTextCalls();
    clearTextCalls();
    clearRects();
    clearLineCalls();
    clearOperations();
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
  void clearScreen(uint8_t color = 0xFF) const {
    clearColor_ = color;
    display_.clearScreen(color);
  }
  void clearArea(int, int, int, int, uint8_t = 0xFF) const {}
  void drawLine(int x0, int y0, int x1, int y1, bool color = true) const {
    lineCalls_.push_back({x0, y0, x1, y1, color});
    DrawCall call;
    call.kind = DrawCall::Kind::Line;
    call.x = x0;
    call.y = y0;
    call.x1 = x1;
    call.y1 = y1;
    call.black = color;
    operations_.push_back(call);
  }
  void drawRect(int x, int y, int w, int h, bool color = true) const {
    drawRects_.push_back({x, y, w, h, color});
    DrawCall call;
    call.kind = DrawCall::Kind::Rect;
    call.x = x;
    call.y = y;
    call.w = w;
    call.h = h;
    call.black = color;
    operations_.push_back(call);
  }
  void fillRect(int x, int y, int w, int h, bool color = true) const {
    fillRects_.push_back({x, y, w, h, color});
    DrawCall call;
    call.kind = DrawCall::Kind::Fill;
    call.x = x;
    call.y = y;
    call.w = w;
    call.h = h;
    call.black = color;
    operations_.push_back(call);
  }
  void drawText(int fontId, int x, int y, const char* text, bool black = true,
                EpdFontFamily::Style style = EpdFontFamily::REGULAR) const {
    lastText_ = text ? text : "";
    textCalls_.push_back({fontId, x, y, lastText_, black, style});
    DrawCall call;
    call.kind = DrawCall::Kind::Text;
    call.fontId = fontId;
    call.x = x;
    call.y = y;
    call.text = lastText_;
    call.black = black;
    call.style = style;
    operations_.push_back(call);
  }
  void drawCenteredText(int fontId, int y, const char* text, bool black = true,
                        EpdFontFamily::Style style = EpdFontFamily::REGULAR) const {
    const std::string value = text ? text : "";
    centeredTextCalls_.push_back({fontId, y, value, black, style});
    DrawCall call;
    call.kind = DrawCall::Kind::Centered;
    call.fontId = fontId;
    call.y = y;
    call.text = value;
    call.black = black;
    call.style = style;
    operations_.push_back(call);
  }
  void drawImage(const uint8_t*, int, int, int, int) const {}
  void displayBuffer(papyrix::hal::Display::RefreshMode = papyrix::hal::Display::FAST_REFRESH, bool = false) const {}

 private:
  papyrix::hal::Display& display_;
  int screenWidth_ = 480;
  int screenHeight_ = 800;
  mutable std::vector<CenteredTextCall> centeredTextCalls_;
  mutable std::vector<TextCall> textCalls_;
  mutable std::vector<RectCall> drawRects_;
  mutable std::vector<RectCall> fillRects_;
  mutable std::vector<LineCall> lineCalls_;
  mutable std::vector<DrawCall> operations_;
  mutable std::string lastText_;
  mutable uint8_t clearColor_ = 0xFF;
  std::vector<std::string> wrappedTextResult_;
};
