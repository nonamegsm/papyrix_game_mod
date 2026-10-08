#include "SolitaireView.h"

#include <GfxRenderer.h>

#include <algorithm>
#include <cstdio>

#include "SolitaireModel.h"
#include "ThemeManager.h"

extern GfxRenderer renderer;

namespace papyrix::games_app::solitaire_view {
namespace {

using games::Solitaire;

constexpr int STOCK_SLOT = -1;
constexpr int AUTO_SLOT = -2;
constexpr int TOP_SLOTS = 7;
constexpr int TABLEAU_COLS = 7;
constexpr int CONTROL_HEIGHT = 72;
constexpr int FOOTER_HEIGHT = 50;

struct Rect {
  int x, y, w, h;
};

struct Layout {
  int w, h;
  int cardW, cardH, gap, left;
  int topY, tableauY, boardBottom, hintY, controlsY;
};

Solitaire model;
int cursorPile = STOCK_SLOT;
int cursorIndex = 0;
int selectedPile = Solitaire::EMPTY;
int selectedIndex = -1;
int scrolls[TABLEAU_COLS] = {};
bool invalidFlash = false;

Layout layout() {
  const int w = renderer.getScreenWidth();
  const int h = renderer.getScreenHeight();
  const int gap = w < 520 ? 5 : 8;
  const int cardW = std::max(42, std::min(w < h ? 66 : 72, (w - 16 - gap * 6) / TOP_SLOTS));
  const int cardH = std::max(58, std::min(h < 550 ? 64 : 88, cardW * 4 / 3));
  const int total = cardW * TOP_SLOTS + gap * (TOP_SLOTS - 1);
  return {w,
          h,
          cardW,
          cardH,
          gap,
          (w - total) / 2,
          80,
          h < 550 ? 174 : 180,
          h - 156,
          h - 140,
          h - FOOTER_HEIGHT - CONTROL_HEIGHT};
}

Rect topRect(int slot, const Layout& l) { return {l.left + slot * (l.cardW + l.gap), l.topY, l.cardW, l.cardH}; }
Rect columnRect(int col, const Layout& l) {
  return {l.left + col * (l.cardW + l.gap), l.tableauY, l.cardW, l.boardBottom - l.tableauY};
}
int topPile(int slot) {
  if (slot == 0) return STOCK_SLOT;
  if (slot == 1) return Solitaire::WASTE;
  if (slot == 2) return AUTO_SLOT;
  return Solitaire::FOUNDATION_FIRST + slot - 3;
}
int topSlotForPile(int pile) {
  if (pile == STOCK_SLOT) return 0;
  if (pile == Solitaire::WASTE) return 1;
  if (pile == AUTO_SLOT) return 2;
  if (pile >= Solitaire::FOUNDATION_FIRST && pile < Solitaire::TABLEAU_FIRST)
    return pile - Solitaire::FOUNDATION_FIRST + 3;
  return 0;
}
bool isTableau(int pile) { return pile >= Solitaire::TABLEAU_FIRST && pile < Solitaire::PILE_COUNT; }
bool isTopCursor() { return !isTableau(cursorPile); }
int pitch() { return std::max(22, renderer.getLineHeight(THEME.smallFontId) + 4); }
int backPitch(const Layout& l) { return l.h < 550 ? 6 : 12; }
int hiddenOffset(int hidden, const Layout& l) { return hidden * backPitch(l); }
int firstFaceUp(int pile) {
  const int count = model.count(pile);
  for (int i = 0; i < count; ++i) {
    if (model.faceUp(pile, i)) return i;
  }
  return count;
}
int visibleCapacity(int pile, const Layout& l) {
  const int hidden = firstFaceUp(pile);
  const int startY = l.tableauY + hiddenOffset(hidden, l);
  return std::max(1, (l.boardBottom - 24 - startY - l.cardH) / pitch() + 1);
}
void clampScroll(int col, const Layout& l) {
  const int pile = Solitaire::TABLEAU_FIRST + col;
  const int face = model.count(pile) - firstFaceUp(pile);
  scrolls[col] = std::max(0, std::min(scrolls[col], std::max(0, face - visibleCapacity(pile, l))));
}
void clearSelection() {
  selectedPile = Solitaire::EMPTY;
  selectedIndex = -1;
}
void clampCursor() {
  const Layout l = layout();
  for (int col = 0; col < TABLEAU_COLS; ++col) clampScroll(col, l);
  if (isTableau(cursorPile)) {
    const int first = firstFaceUp(cursorPile);
    const int count = model.count(cursorPile);
    cursorIndex = count <= first ? first : std::max(first, std::min(cursorIndex, count - 1));
  }
}
void ensureCursorVisible() {
  if (!isTableau(cursorPile)) return;
  const Layout l = layout();
  const int col = cursorPile - Solitaire::TABLEAU_FIRST;
  const int first = firstFaceUp(cursorPile);
  const int visible = visibleCapacity(cursorPile, l);
  const int offset = std::max(0, cursorIndex - first);
  if (offset < scrolls[col]) scrolls[col] = offset;
  if (offset >= scrolls[col] + visible) scrolls[col] = offset - visible + 1;
  clampScroll(col, l);
}

const char* rankText(uint8_t card) {
  static char text[3];
  const int rank = Solitaire::rank(card);
  if (rank == 1) return "A";
  if (rank == 11) return "J";
  if (rank == 12) return "Q";
  if (rank == 13) return "K";
  std::snprintf(text, sizeof(text), "%d", rank);
  return text;
}
const char* suitText(uint8_t card) {
  constexpr const char* suits[] = {"C", "D", "H", "S"};
  const int suit = Solitaire::suit(card);
  return suit >= 0 && suit < 4 ? suits[suit] : "?";
}
void centerText(const Rect& r, const char* text, int font = -1) {
  const int f = font < 0 ? THEME.smallFontId : font;
  renderer.drawText(f, r.x + (r.w - renderer.getTextWidth(f, text)) / 2, r.y + (r.h - renderer.getLineHeight(f)) / 2,
                    text, THEME.primaryTextBlack);
}
void drawBack(const Rect& r) {
  renderer.fillRect(r.x, r.y, r.w, r.h, !THEME.primaryTextBlack);
  renderer.drawRect(r.x, r.y, r.w, r.h, THEME.primaryTextBlack);
  for (int y = r.y + 6; y < r.y + r.h - 4; y += 10) {
    renderer.drawLine(r.x + 5, y, r.x + r.w - 6, y + 5, THEME.primaryTextBlack);
  }
}
void drawSuitMark(const Rect& r, uint8_t card) {
  const int cx = r.x + r.w / 2;
  const int cy = r.y + r.h / 2 + 6;
  const int suit = Solitaire::suit(card);
  if (suit == 1) {
    renderer.drawLine(cx, cy - 9, cx + 9, cy, THEME.primaryTextBlack);
    renderer.drawLine(cx + 9, cy, cx, cy + 9, THEME.primaryTextBlack);
    renderer.drawLine(cx, cy + 9, cx - 9, cy, THEME.primaryTextBlack);
    renderer.drawLine(cx - 9, cy, cx, cy - 9, THEME.primaryTextBlack);
  } else if (suit == 2) {
    renderer.drawLine(cx - 9, cy - 2, cx - 3, cy - 9, THEME.primaryTextBlack);
    renderer.drawLine(cx - 3, cy - 9, cx, cy - 3, THEME.primaryTextBlack);
    renderer.drawLine(cx, cy - 3, cx + 3, cy - 9, THEME.primaryTextBlack);
    renderer.drawLine(cx + 3, cy - 9, cx + 9, cy - 2, THEME.primaryTextBlack);
    renderer.drawLine(cx - 9, cy - 2, cx, cy + 10, THEME.primaryTextBlack);
    renderer.drawLine(cx + 9, cy - 2, cx, cy + 10, THEME.primaryTextBlack);
  } else if (suit == 3) {
    renderer.fillRect(cx - 6, cy - 9, 12, 14, THEME.primaryTextBlack);
    renderer.fillRect(cx - 9, cy - 2, 18, 7, THEME.primaryTextBlack);
    renderer.fillRect(cx - 2, cy + 4, 4, 8, THEME.primaryTextBlack);
  } else {
    renderer.fillRect(cx - 5, cy - 8, 10, 16, THEME.primaryTextBlack);
    renderer.fillRect(cx - 9, cy - 3, 18, 8, THEME.primaryTextBlack);
  }
}
void drawCard(const Rect& r, uint8_t card, bool faceUp, bool cursor, bool selected) {
  if (!faceUp) {
    drawBack(r);
  } else {
    renderer.fillRect(r.x, r.y, r.w, r.h, !THEME.primaryTextBlack);
    renderer.drawRect(r.x, r.y, r.w, r.h, THEME.primaryTextBlack);
    char label[8];
    std::snprintf(label, sizeof(label), "%s%s", rankText(card), suitText(card));
    renderer.drawText(THEME.smallFontId, r.x + 4, r.y + 4, label, THEME.primaryTextBlack);
    drawSuitMark(r, card);
  }
  if (selected) {
    renderer.drawRect(r.x - 3, r.y - 3, r.w + 6, r.h + 6, THEME.primaryTextBlack);
    renderer.drawRect(r.x - 5, r.y - 5, r.w + 10, r.h + 10, THEME.primaryTextBlack);
  } else if (cursor) {
    renderer.drawRect(r.x - 3, r.y - 3, r.w + 6, r.h + 6, THEME.primaryTextBlack);
  }
}
void drawEmpty(const Rect& r, const char* label, bool cursor) {
  renderer.fillRect(r.x, r.y, r.w, r.h, !THEME.primaryTextBlack);
  renderer.drawRect(r.x, r.y, r.w, r.h, THEME.secondaryTextBlack);
  centerText(r, label);
  if (cursor) renderer.drawRect(r.x - 3, r.y - 3, r.w + 6, r.h + 6, THEME.primaryTextBlack);
}
void drawButton(const Rect& r, const char* label, bool enabled = true) {
  renderer.drawRect(r.x, r.y, r.w, r.h, THEME.primaryTextBlack);
  centerText(r, label, enabled ? THEME.uiFontId : THEME.smallFontId);
}
bool tryMoveTo(int targetPile) {
  if (selectedPile == Solitaire::EMPTY) return false;
  if (targetPile == selectedPile) {
    clearSelection();
    return true;
  }
  const bool moved = targetPile >= 0 && model.move(selectedPile, selectedIndex, targetPile);
  if (moved) {
    clearSelection();
  } else {
    invalidFlash = true;
  }
  clampCursor();
  return true;
}
bool activatePile(int pile, int index) {
  invalidFlash = false;
  if (pile == STOCK_SLOT) {
    clearSelection();
    invalidFlash = !model.draw();
    return true;
  }
  if (pile == AUTO_SLOT) {
    clearSelection();
    invalidFlash = !model.autoMove();
    return true;
  }
  if (selectedPile != Solitaire::EMPTY) return tryMoveTo(pile);
  if (pile < 0 || model.count(pile) == 0) {
    invalidFlash = true;
    return true;
  }
  if (pile == Solitaire::WASTE || (pile >= Solitaire::FOUNDATION_FIRST && pile < Solitaire::TABLEAU_FIRST)) {
    index = model.count(pile) - 1;
  }
  if (model.faceUp(pile, index)) {
    selectedPile = pile;
    selectedIndex = index;
    return true;
  }
  invalidFlash = true;
  return true;
}
void drawTop(const Layout& l) {
  for (int slot = 0; slot < TOP_SLOTS; ++slot) {
    const int pile = topPile(slot);
    const Rect r = topRect(slot, l);
    const bool cursor = isTopCursor() && topSlotForPile(cursorPile) == slot;
    const bool selected = selectedPile == pile;
    if (pile == STOCK_SLOT) {
      if (model.stockCount() > 0)
        drawBack(r);
      else
        drawEmpty(r, "Stock", cursor);
      if (cursor && model.stockCount() > 0)
        renderer.drawRect(r.x - 3, r.y - 3, r.w + 6, r.h + 6, THEME.primaryTextBlack);
      char count[8];
      std::snprintf(count, sizeof(count), "%d", model.stockCount());
      renderer.drawText(THEME.smallFontId, r.x + 4, r.y + r.h - 22, count, THEME.primaryTextBlack);
    } else if (pile == AUTO_SLOT) {
      drawEmpty(r, "Auto", cursor);
    } else if (model.count(pile) > 0) {
      drawCard(r, model.card(pile, model.count(pile) - 1), true, cursor, selected);
    } else {
      drawEmpty(r, pile == Solitaire::WASTE ? "Waste" : "F", cursor);
    }
  }
}
void drawTableau(const Layout& l) {
  for (int col = 0; col < TABLEAU_COLS; ++col) {
    const int pile = Solitaire::TABLEAU_FIRST + col;
    clampScroll(col, l);
    const Rect cr = columnRect(col, l);
    const int first = firstFaceUp(pile);
    const int count = model.count(pile);
    if (count == 0) drawEmpty({cr.x, cr.y, l.cardW, l.cardH}, "K", cursorPile == pile);
    for (int i = 0; i < first; ++i) {
      drawBack({cr.x, cr.y + i * backPitch(l), l.cardW, l.cardH});
    }
    const int startY = cr.y + hiddenOffset(first, l);
    const int visible = visibleCapacity(pile, l);
    const int begin = first + scrolls[col];
    const int end = std::min(count, begin + visible);
    const int p = pitch();
    for (int i = begin; i < end; ++i) {
      const Rect r{cr.x, startY + (i - begin) * p, l.cardW, l.cardH};
      drawCard(r, model.card(pile, i), true, cursorPile == pile && cursorIndex == i,
               selectedPile == pile && selectedIndex == i);
    }
    if (count - first > visible) {
      const int y = l.boardBottom - 19;
      if (scrolls[col] > 0) centerText({cr.x, y, l.cardW / 2, 16}, "U");
      if (scrolls[col] + visible < count - first) centerText({cr.x + l.cardW / 2, y, l.cardW / 2, 16}, "D");
    }
  }
}

}  // namespace

void reset(uint32_t seed) {
  model.reset(seed);
  cursorPile = STOCK_SLOT;
  cursorIndex = 0;
  clearSelection();
  std::fill(std::begin(scrolls), std::end(scrolls), 0);
  invalidFlash = false;
}

bool handleButton(Button button) {
  invalidFlash = false;
  if (button == Button::Back || button == Button::Power || button == Button::Count) return false;
  if (button == Button::Left || button == Button::Right) {
    const int delta = button == Button::Left ? -1 : 1;
    if (isTopCursor()) {
      cursorPile = topPile((topSlotForPile(cursorPile) + TOP_SLOTS + delta) % TOP_SLOTS);
    } else {
      int col = cursorPile - Solitaire::TABLEAU_FIRST + delta;
      if (col < 0) {
        cursorPile = STOCK_SLOT;
      } else if (col >= TABLEAU_COLS) {
        cursorPile = Solitaire::TABLEAU_FIRST;
      } else {
        cursorPile = Solitaire::TABLEAU_FIRST + col;
      }
    }
    clampCursor();
    ensureCursorVisible();
    return true;
  }
  if (button == Button::Up) {
    if (isTableau(cursorPile)) {
      const int first = firstFaceUp(cursorPile);
      if (cursorIndex <= first) {
        cursorPile = topPile(cursorPile - Solitaire::TABLEAU_FIRST);
      } else {
        --cursorIndex;
      }
      ensureCursorVisible();
    }
    return true;
  }
  if (button == Button::Down) {
    if (isTopCursor()) {
      int col = topSlotForPile(cursorPile);
      if (col >= TABLEAU_COLS) col = TABLEAU_COLS - 1;
      cursorPile = Solitaire::TABLEAU_FIRST + col;
      cursorIndex = firstFaceUp(cursorPile);
    } else if (cursorIndex + 1 < model.count(cursorPile)) {
      ++cursorIndex;
    }
    clampCursor();
    ensureCursorVisible();
    return true;
  }
  if (button == Button::Center) {
    return activatePile(cursorPile, cursorIndex);
  }
  return false;
}

bool tap(int x, int y) {
  const Layout l = layout();
  if (x < 0 || x >= l.w || y < 0 || y >= l.h) return false;
  invalidFlash = false;
  for (int slot = 0; slot < TOP_SLOTS; ++slot) {
    const Rect r = topRect(slot, l);
    if (x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h) {
      cursorPile = topPile(slot);
      cursorIndex = 0;
      return activatePile(cursorPile, cursorIndex);
    }
  }
  for (int col = 0; col < TABLEAU_COLS; ++col) {
    const int pile = Solitaire::TABLEAU_FIRST + col;
    const Rect cr = columnRect(col, l);
    if (x < cr.x || x >= cr.x + cr.w || y < cr.y || y >= cr.y + cr.h) continue;
    clampScroll(col, l);
    const int first = firstFaceUp(pile);
    const int visible = visibleCapacity(pile, l);
    if (model.count(pile) - first > visible && y >= l.boardBottom - 24) {
      if (x < cr.x + cr.w / 2 && scrolls[col] > 0) --scrolls[col];
      if (x >= cr.x + cr.w / 2) ++scrolls[col];
      clampScroll(col, l);
      return true;
    }
    const int startY = cr.y + hiddenOffset(first, l);
    if (first > 0 && y < startY) {
      if (selectedPile == Solitaire::EMPTY) return false;
      cursorPile = pile;
      cursorIndex = first;
      return activatePile(pile, cursorIndex);
    }
    const int begin = first + scrolls[col];
    const int end = std::min(model.count(pile), begin + visible);
    const int lastBottom =
        model.count(pile) == 0 ? cr.y + l.cardH : startY + std::max(0, end - begin - 1) * pitch() + l.cardH;
    if (y >= lastBottom && selectedPile == Solitaire::EMPTY) return false;
    if (y >= lastBottom) {
      cursorPile = pile;
      cursorIndex = end > begin ? end - 1 : first;
      return activatePile(pile, cursorIndex);
    }
    int index = model.count(pile) == 0 ? first : begin + std::max(0, (y - startY) / pitch());
    index = std::min(index, std::max(first, end - 1));
    cursorPile = pile;
    cursorIndex = std::max(first, index);
    ensureCursorVisible();
    return activatePile(pile, cursorIndex);
  }
  const int bw = l.w / 3;
  if (y >= l.controlsY && y < l.controlsY + CONTROL_HEIGHT) {
    if (x < bw) return activatePile(STOCK_SLOT, 0);
    if (x < bw * 2) return undo();
    return activatePile(AUTO_SLOT, 0);
  }
  return false;
}

void renderBoard() {
  const Layout l = layout();
  clampCursor();
  char hud[48];
  std::snprintf(hud, sizeof(hud), "Moves: %lu", static_cast<unsigned long>(model.moves()));
  renderer.drawCenteredText(THEME.uiFontId, 52, won() ? "You won! Menu or Undo" : hud, THEME.primaryTextBlack);
  drawTop(l);
  drawTableau(l);
  renderer.drawCenteredText(THEME.smallFontId, l.hintY,
                            invalidFlash ? "No legal move" : "Center/tap selects, Stock draws, Auto builds",
                            THEME.primaryTextBlack);
  const int bw = l.w / 3;
  drawButton({4, l.controlsY + 4, bw - 8, CONTROL_HEIGHT - 8}, "Stock");
  drawButton({bw + 4, l.controlsY + 4, bw - 8, CONTROL_HEIGHT - 8}, "Undo", model.canUndo());
  drawButton({bw * 2 + 4, l.controlsY + 4, l.w - bw * 2 - 8, CONTROL_HEIGHT - 8}, "Auto");
}

bool won() { return model.won(); }

bool undo() {
  if (!model.canUndo()) {
    const bool changed = selectedPile != Solitaire::EMPTY || invalidFlash;
    clearSelection();
    invalidFlash = false;
    return changed;
  }
  clearSelection();
  invalidFlash = false;
  const bool changed = model.undo();
  clampCursor();
  ensureCursorVisible();
  return changed;
}

}  // namespace papyrix::games_app::solitaire_view
