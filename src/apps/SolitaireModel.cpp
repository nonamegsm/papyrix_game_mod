#include "SolitaireModel.h"

#include <algorithm>

namespace papyrix::games {

uint32_t Solitaire::nextRandom() {
  if (rng_ == 0) rng_ = 1;
  uint32_t x = rng_;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  rng_ = x == 0 ? 1 : x;
  return rng_;
}

void Solitaire::clearState() { state_ = State{}; }

void Solitaire::reset(uint32_t seed) {
  rng_ = seed == 0 ? 1 : seed;
  clearState();
  historyNext_ = 0;
  historyCount_ = 0;

  uint8_t deck[MAX_CARDS] = {};
  for (uint8_t i = 0; i < MAX_CARDS; i++) {
    deck[i] = i;
  }
  for (int i = MAX_CARDS - 1; i > 0; i--) {
    const int j = static_cast<int>(nextRandom() % static_cast<uint32_t>(i + 1));
    std::swap(deck[i], deck[j]);
  }

  int next = 0;
  for (int col = 0; col < 7; col++) {
    const int pile = TABLEAU_FIRST + col;
    for (int row = 0; row <= col; row++) {
      const int index = state_.counts[pile]++;
      state_.cards[pile][index] = deck[next++];
      setFaceUp(state_, pile, index, row == col);
    }
  }

  state_.stockCount = static_cast<uint8_t>(MAX_CARDS - next);
  for (int i = 0; i < state_.stockCount; i++) {
    state_.stock[i] = deck[next + state_.stockCount - 1 - i];
  }
}

int Solitaire::count(int pile) const {
  if (!validPile(pile)) return 0;
  return state_.counts[pile];
}

uint8_t Solitaire::card(int pile, int index) const {
  if (!validPile(pile) || index < 0 || index >= state_.counts[pile]) return EMPTY;
  return state_.cards[pile][index];
}

bool Solitaire::faceUp(int pile, int index) const {
  if (!validPile(pile) || index < 0 || index >= state_.counts[pile]) return false;
  return isFaceUpInState(state_, pile, index);
}

bool Solitaire::draw() {
  if (state_.stockCount == 0 && state_.counts[WASTE] == 0) return false;

  pushHistory();
  if (state_.stockCount == 0) {
    const uint8_t wasteCount = state_.counts[WASTE];
    for (uint8_t i = 0; i < wasteCount; i++) {
      state_.stock[i] = state_.cards[WASTE][wasteCount - 1 - i];
    }
    state_.stockCount = wasteCount;
    state_.counts[WASTE] = 0;
    state_.faceMask[WASTE] = 0;
  } else {
    const uint8_t drawn = state_.stock[--state_.stockCount];
    const int index = state_.counts[WASTE]++;
    state_.cards[WASTE][index] = drawn;
    setFaceUp(state_, WASTE, index, true);
  }
  state_.moves++;
  return true;
}

bool Solitaire::move(int fromPile, int firstCard, int toPile) {
  if (!validSourceSelection(fromPile, firstCard, toPile)) return false;

  const uint8_t movingCard = state_.cards[fromPile][firstCard];
  const int movingCount = state_.counts[fromPile] - firstCard;
  bool accepted = false;

  if (isFoundation(toPile)) {
    accepted = movingCount == 1 && !isFoundation(fromPile) && canMoveToFoundation(movingCard, toPile);
  } else if (isTableau(toPile)) {
    accepted = canMoveToTableau(movingCard, toPile);
  }
  if (!accepted) return false;

  pushHistory();
  const int destStart = state_.counts[toPile];
  for (int i = 0; i < movingCount; i++) {
    state_.cards[toPile][destStart + i] = state_.cards[fromPile][firstCard + i];
    setFaceUp(state_, toPile, destStart + i, true);
  }
  state_.counts[toPile] = static_cast<uint8_t>(state_.counts[toPile] + movingCount);

  for (int i = firstCard; i < state_.counts[fromPile]; i++) {
    setFaceUp(state_, fromPile, i, false);
  }
  state_.counts[fromPile] = static_cast<uint8_t>(firstCard);
  if (isTableau(fromPile)) revealTableauTop(fromPile);

  state_.moves++;
  return true;
}

bool Solitaire::autoMove() {
  const int wasteTop = state_.counts[WASTE] - 1;
  if (wasteTop >= 0) {
    for (int foundation = FOUNDATION_FIRST; foundation < TABLEAU_FIRST; foundation++) {
      if (canMoveToFoundation(state_.cards[WASTE][wasteTop], foundation)) {
        return move(WASTE, wasteTop, foundation);
      }
    }
  }

  for (int pile = TABLEAU_FIRST; pile < PILE_COUNT; pile++) {
    const int top = state_.counts[pile] - 1;
    if (top < 0 || !faceUp(pile, top)) continue;
    for (int foundation = FOUNDATION_FIRST; foundation < TABLEAU_FIRST; foundation++) {
      if (canMoveToFoundation(state_.cards[pile][top], foundation)) {
        return move(pile, top, foundation);
      }
    }
  }
  return false;
}

bool Solitaire::undo() {
  if (!canUndo()) return false;
  const uint8_t previous = static_cast<uint8_t>((historyNext_ + HISTORY - 1) % HISTORY);
  state_ = history_[previous];
  historyNext_ = previous;
  historyCount_--;
  return true;
}

bool Solitaire::won() const {
  for (int pile = FOUNDATION_FIRST; pile < TABLEAU_FIRST; pile++) {
    if (state_.counts[pile] != 13) return false;
  }
  return true;
}

void Solitaire::pushHistory() {
  history_[historyNext_] = state_;
  historyNext_ = static_cast<uint8_t>((historyNext_ + 1) % HISTORY);
  if (historyCount_ < HISTORY) historyCount_++;
}

bool Solitaire::validPile(int pile) const { return pile >= 0 && pile < PILE_COUNT; }

bool Solitaire::isFoundation(int pile) const { return pile >= FOUNDATION_FIRST && pile < TABLEAU_FIRST; }

bool Solitaire::isTableau(int pile) const { return pile >= TABLEAU_FIRST && pile < PILE_COUNT; }

bool Solitaire::isFaceUpInState(const State& state, int pile, int index) const {
  return (state.faceMask[pile] & (uint64_t(1) << index)) != 0;
}

void Solitaire::setFaceUp(State& state, int pile, int index, bool value) {
  const uint64_t bit = uint64_t(1) << index;
  if (value) {
    state.faceMask[pile] |= bit;
  } else {
    state.faceMask[pile] &= ~bit;
  }
}

bool Solitaire::canMoveToTableau(uint8_t movingCard, int toPile) const {
  if (!isTableau(toPile) || movingCard == EMPTY) return false;
  const int toCount = state_.counts[toPile];
  if (toCount == 0) return rank(movingCard) == 13;
  const uint8_t target = state_.cards[toPile][toCount - 1];
  return faceUp(toPile, toCount - 1) && red(movingCard) != red(target) && rank(movingCard) + 1 == rank(target);
}

bool Solitaire::canMoveToFoundation(uint8_t movingCard, int toPile) const {
  if (!isFoundation(toPile) || movingCard == EMPTY) return false;
  const int toCount = state_.counts[toPile];
  if (toCount == 0) return rank(movingCard) == 1;
  const uint8_t target = state_.cards[toPile][toCount - 1];
  return suit(movingCard) == suit(target) && rank(movingCard) == rank(target) + 1;
}

bool Solitaire::validSourceSelection(int fromPile, int firstCard, int toPile) const {
  if (!validPile(fromPile) || !validPile(toPile) || fromPile == toPile) return false;
  if (firstCard < 0 || firstCard >= state_.counts[fromPile]) return false;
  if (fromPile == WASTE || isFoundation(fromPile)) {
    return firstCard == state_.counts[fromPile] - 1;
  }
  if (!isTableau(fromPile) || !faceUp(fromPile, firstCard)) return false;
  for (int i = firstCard; i + 1 < state_.counts[fromPile]; i++) {
    const uint8_t upper = state_.cards[fromPile][i];
    const uint8_t lower = state_.cards[fromPile][i + 1];
    if (!faceUp(fromPile, i + 1) || red(upper) == red(lower) || rank(upper) != rank(lower) + 1) {
      return false;
    }
  }
  return true;
}

void Solitaire::revealTableauTop(int pile) {
  const int top = state_.counts[pile] - 1;
  if (top >= 0 && !faceUp(pile, top)) setFaceUp(state_, pile, top, true);
}

}  // namespace papyrix::games
