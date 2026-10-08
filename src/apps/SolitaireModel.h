#pragma once

#include <cstdint>

namespace papyrix::games {

struct SolitaireTestAccess;

class Solitaire {
 public:
  static constexpr uint8_t EMPTY = 255;
  static constexpr int WASTE = 0;
  static constexpr int FOUNDATION_FIRST = 1;
  static constexpr int TABLEAU_FIRST = 5;
  static constexpr int PILE_COUNT = 12;
  static constexpr int HISTORY = 8;

  void reset(uint32_t seed = 1);
  int count(int pile) const;
  uint8_t card(int pile, int index) const;
  bool faceUp(int pile, int index) const;
  int stockCount() const { return state_.stockCount; }
  bool draw();
  bool move(int fromPile, int firstCard, int toPile);
  bool autoMove();
  bool undo();
  bool canUndo() const { return historyCount_ > 0; }
  uint32_t moves() const { return state_.moves; }
  bool won() const;

  static int rank(uint8_t card) { return card == EMPTY ? 0 : static_cast<int>(card % 13) + 1; }
  static int suit(uint8_t card) { return card == EMPTY ? -1 : static_cast<int>(card / 13); }
  static bool red(uint8_t card) { return suit(card) == 1 || suit(card) == 2; }

 private:
  friend struct SolitaireTestAccess;

  static constexpr int MAX_CARDS = 52;

  struct State {
    uint8_t cards[PILE_COUNT][MAX_CARDS] = {};
    uint64_t faceMask[PILE_COUNT] = {};
    uint8_t counts[PILE_COUNT] = {};
    uint8_t stock[MAX_CARDS] = {};
    uint8_t stockCount = 0;
    uint32_t moves = 0;
  };

  State state_ = {};
  State history_[HISTORY] = {};
  uint8_t historyNext_ = 0;
  uint8_t historyCount_ = 0;
  uint32_t rng_ = 1;

  uint32_t nextRandom();
  void clearState();
  void pushHistory();
  bool validPile(int pile) const;
  bool isFoundation(int pile) const;
  bool isTableau(int pile) const;
  bool isFaceUpInState(const State& state, int pile, int index) const;
  void setFaceUp(State& state, int pile, int index, bool value);
  bool canMoveToTableau(uint8_t movingCard, int toPile) const;
  bool canMoveToFoundation(uint8_t movingCard, int toPile) const;
  bool validSourceSelection(int fromPile, int firstCard, int toPile) const;
  void revealTableauTop(int pile);
};

}  // namespace papyrix::games
