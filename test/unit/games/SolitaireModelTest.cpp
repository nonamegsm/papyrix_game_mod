#include <apps/SolitaireModel.h>

#include <cstring>

#include "test_utils.h"

using papyrix::games::Solitaire;

namespace papyrix::games {

struct SolitaireTestAccess {
  static void clear(Solitaire& game) {
    game.clearState();
    game.historyNext_ = 0;
    game.historyCount_ = 0;
  }

  static void setPile(Solitaire& game, int pile, const uint8_t* cards, const bool* faceUp, int count) {
    game.state_.counts[pile] = static_cast<uint8_t>(count);
    game.state_.faceMask[pile] = 0;
    for (int i = 0; i < count; i++) {
      game.state_.cards[pile][i] = cards[i];
      game.setFaceUp(game.state_, pile, i, faceUp == nullptr || faceUp[i]);
    }
  }

  static void setStock(Solitaire& game, const uint8_t* cards, int count) {
    game.state_.stockCount = static_cast<uint8_t>(count);
    for (int i = 0; i < count; i++) {
      game.state_.stock[i] = cards[i];
    }
  }

  static uint8_t stockCard(const Solitaire& game, int index) { return game.state_.stock[index]; }
  static void setMoves(Solitaire& game, uint32_t moves) { game.state_.moves = moves; }
  static void setHistoryCount(Solitaire& game, uint8_t count) { game.historyCount_ = count; }
};

}  // namespace papyrix::games

namespace {

uint8_t card(int suit, int rank) { return static_cast<uint8_t>(suit * 13 + rank - 1); }

int totalCards(const Solitaire& game) {
  int total = game.stockCount();
  for (int pile = 0; pile < Solitaire::PILE_COUNT; pile++) {
    total += game.count(pile);
  }
  return total;
}

bool allCardsUnique(const Solitaire& game) {
  bool seen[52] = {};
  for (int i = 0; i < game.stockCount(); i++) {
    const uint8_t c = papyrix::games::SolitaireTestAccess::stockCard(game, i);
    if (c >= 52 || seen[c]) return false;
    seen[c] = true;
  }
  for (int pile = 0; pile < Solitaire::PILE_COUNT; pile++) {
    for (int i = 0; i < game.count(pile); i++) {
      const uint8_t c = game.card(pile, i);
      if (c >= 52 || seen[c]) return false;
      seen[c] = true;
    }
  }
  return true;
}

uint8_t topCard(const Solitaire& game, int pile) { return game.card(pile, game.count(pile) - 1); }

bool sameLayout(const Solitaire& left, const Solitaire& right) {
  if (left.stockCount() != right.stockCount() || left.moves() != right.moves()) return false;
  for (int pile = 0; pile < Solitaire::PILE_COUNT; pile++) {
    if (left.count(pile) != right.count(pile)) return false;
    for (int i = 0; i < left.count(pile); i++) {
      if (left.card(pile, i) != right.card(pile, i) || left.faceUp(pile, i) != right.faceUp(pile, i)) return false;
    }
  }
  return true;
}

void expectConserved(TestUtils::TestRunner& runner, const Solitaire& game, const std::string& label) {
  runner.expectEq(52, totalCards(game), label + " keeps 52 cards");
  runner.expectTrue(allCardsUnique(game), label + " keeps all cards unique");
}

}  // namespace

int main() {
  TestUtils::TestRunner runner("SolitaireModelTest");

  {
    Solitaire game;
    game.reset(42);
    Solitaire same;
    same.reset(42);
    Solitaire different;
    different.reset(43);

    runner.expectEq(24, game.stockCount(), "deal leaves 24 cards in stock");
    for (int col = 0; col < 7; col++) {
      const int pile = Solitaire::TABLEAU_FIRST + col;
      runner.expectEq(col + 1, game.count(pile), "deal sizes tableau columns");
      for (int i = 0; i < game.count(pile); i++) {
        runner.expectEq(i == game.count(pile) - 1, game.faceUp(pile, i), "deal exposes only tableau tops");
      }
    }
    runner.expectEq(topCard(same, Solitaire::TABLEAU_FIRST), topCard(game, Solitaire::TABLEAU_FIRST),
                    "same seed gives deterministic deal");
    runner.expectNe(topCard(different, Solitaire::TABLEAU_FIRST), topCard(game, Solitaire::TABLEAU_FIRST),
                    "different seed changes deal");
    expectConserved(runner, game, "initial deal");
  }

  {
    Solitaire game;
    game.reset(5);
    uint8_t drawn[24] = {};
    for (int i = 0; i < 24; i++) {
      runner.expectTrue(game.draw(), "stock draw succeeds");
      drawn[i] = topCard(game, Solitaire::WASTE);
    }
    runner.expectEq(0, game.stockCount(), "drawing all stock empties stock");
    runner.expectEq(24, game.count(Solitaire::WASTE), "drawing all stock fills waste");
    runner.expectTrue(game.draw(), "empty stock recycles waste");
    runner.expectEq(24, game.stockCount(), "recycle restores stock count");
    runner.expectEq(0, game.count(Solitaire::WASTE), "recycle clears waste");
    runner.expectTrue(game.draw(), "draw after recycle succeeds");
    runner.expectEq(drawn[0], topCard(game, Solitaire::WASTE), "recycle preserves original draw order");
    expectConserved(runner, game, "draw and recycle");
  }

  {
    Solitaire game;
    papyrix::games::SolitaireTestAccess::clear(game);
    const bool up[] = {true, true};
    const uint8_t source[] = {card(0, 6), card(1, 5)};
    const uint8_t target[] = {card(2, 7)};
    papyrix::games::SolitaireTestAccess::setPile(game, Solitaire::TABLEAU_FIRST, source, up, 2);
    papyrix::games::SolitaireTestAccess::setPile(game, Solitaire::TABLEAU_FIRST + 1, target, up, 1);

    runner.expectTrue(game.move(Solitaire::TABLEAU_FIRST, 0, Solitaire::TABLEAU_FIRST + 1),
                      "alternating descending stack moves to tableau");
    runner.expectEq(3, game.count(Solitaire::TABLEAU_FIRST + 1), "multi-card stack arrives intact");
    runner.expectEq(card(1, 5), topCard(game, Solitaire::TABLEAU_FIRST + 1), "multi-card stack preserves order");
    Solitaire invalid;
    papyrix::games::SolitaireTestAccess::clear(invalid);
    const uint8_t blackSix[] = {card(0, 6)};
    const uint8_t blackSeven[] = {card(3, 7)};
    papyrix::games::SolitaireTestAccess::setPile(invalid, Solitaire::TABLEAU_FIRST, blackSix, up, 1);
    papyrix::games::SolitaireTestAccess::setPile(invalid, Solitaire::TABLEAU_FIRST + 1, blackSeven, up, 1);
    runner.expectFalse(invalid.move(Solitaire::TABLEAU_FIRST, 0, Solitaire::TABLEAU_FIRST + 1),
                       "same-color tableau move is rejected");
  }

  {
    Solitaire game;
    papyrix::games::SolitaireTestAccess::clear(game);
    const bool hiddenThenUp[] = {false, true};
    const bool up[] = {true};
    const uint8_t stack[] = {card(0, 6), card(1, 5)};
    const uint8_t blackSix[] = {card(3, 6)};
    const uint8_t king[] = {card(2, 13)};
    papyrix::games::SolitaireTestAccess::setPile(game, Solitaire::TABLEAU_FIRST, stack, hiddenThenUp, 2);
    papyrix::games::SolitaireTestAccess::setPile(game, Solitaire::TABLEAU_FIRST + 1, blackSix, up, 1);
    papyrix::games::SolitaireTestAccess::setPile(game, Solitaire::TABLEAU_FIRST + 2, king, up, 1);

    runner.expectFalse(game.move(Solitaire::TABLEAU_FIRST, 0, Solitaire::TABLEAU_FIRST + 1),
                       "hidden tableau card cannot be selected");
    runner.expectTrue(game.move(Solitaire::TABLEAU_FIRST, 1, Solitaire::TABLEAU_FIRST + 1),
                      "visible tableau top can move");
    runner.expectTrue(game.faceUp(Solitaire::TABLEAU_FIRST, 0), "moving from tableau reveals exposed hidden top");
    runner.expectFalse(game.move(Solitaire::TABLEAU_FIRST, 0, Solitaire::TABLEAU_FIRST + 3),
                       "non-king cannot move to empty tableau");
    runner.expectTrue(game.move(Solitaire::TABLEAU_FIRST + 2, 0, Solitaire::TABLEAU_FIRST + 3),
                      "king can move to empty tableau");
  }

  {
    Solitaire game;
    papyrix::games::SolitaireTestAccess::clear(game);
    const bool up[] = {true};
    const uint8_t ace[] = {card(2, 1)};
    const uint8_t twoHearts[] = {card(2, 2)};
    const uint8_t twoSpades[] = {card(3, 2)};
    papyrix::games::SolitaireTestAccess::setPile(game, Solitaire::WASTE, ace, up, 1);
    papyrix::games::SolitaireTestAccess::setPile(game, Solitaire::TABLEAU_FIRST, twoHearts, up, 1);
    papyrix::games::SolitaireTestAccess::setPile(game, Solitaire::TABLEAU_FIRST + 1, twoSpades, up, 1);

    runner.expectTrue(game.move(Solitaire::WASTE, 0, Solitaire::FOUNDATION_FIRST), "ace starts foundation");
    runner.expectFalse(game.move(Solitaire::TABLEAU_FIRST + 1, 0, Solitaire::FOUNDATION_FIRST),
                       "foundation rejects wrong suit");
    runner.expectTrue(game.move(Solitaire::TABLEAU_FIRST, 0, Solitaire::FOUNDATION_FIRST),
                      "foundation accepts matching ascending suit");
    runner.expectFalse(game.move(Solitaire::FOUNDATION_FIRST, 1, Solitaire::FOUNDATION_FIRST + 1),
                       "foundation to foundation is rejected");
  }

  {
    Solitaire game;
    papyrix::games::SolitaireTestAccess::clear(game);
    const bool up[] = {true};
    const uint8_t ace[] = {card(0, 1)};
    const uint8_t two[] = {card(0, 2)};
    papyrix::games::SolitaireTestAccess::setPile(game, Solitaire::WASTE, ace, up, 1);
    papyrix::games::SolitaireTestAccess::setPile(game, Solitaire::TABLEAU_FIRST, two, up, 1);

    runner.expectTrue(game.autoMove(), "autoMove moves waste ace to foundation");
    runner.expectTrue(game.autoMove(), "autoMove moves tableau top to foundation");
    runner.expectEq(2, game.count(Solitaire::FOUNDATION_FIRST), "autoMove builds foundation");
    runner.expectFalse(game.autoMove(), "autoMove ignores foundation back moves");
  }

  {
    Solitaire game;
    game.reset(7);
    Solitaire original = game;
    runner.expectFalse(game.move(-1, 0, Solitaire::TABLEAU_FIRST), "invalid source pile is rejected");
    runner.expectFalse(game.move(Solitaire::TABLEAU_FIRST, -1, Solitaire::TABLEAU_FIRST + 1),
                       "invalid source index is rejected");
    runner.expectFalse(game.move(Solitaire::TABLEAU_FIRST, 0, 99), "invalid target pile is rejected");
    runner.expectFalse(game.canUndo(), "failed actions do not create undo history");
    runner.expectTrue(sameLayout(original, game), "invalid moves leave layout unchanged");
  }

  {
    Solitaire game;
    game.reset(9);
    runner.expectTrue(game.draw(), "accepted draw records history");
    const uint8_t drawn = topCard(game, Solitaire::WASTE);
    runner.expectEq(uint32_t(1), game.moves(), "accepted draw increments moves");
    runner.expectTrue(game.undo(), "undo reverts draw");
    runner.expectEq(0, game.count(Solitaire::WASTE), "undo restores waste count");
    runner.expectEq(24, game.stockCount(), "undo restores stock count");
    runner.expectEq(uint32_t(0), game.moves(), "undo restores move counter");
    runner.expectFalse(game.undo(), "undo stops after history is exhausted");

    for (int i = 0; i < 9; i++) {
      runner.expectTrue(game.draw(), "draw fills undo ring");
    }
    runner.expectEq(drawn, game.card(Solitaire::WASTE, 0), "older drawn card is still in waste before ring undo");
    for (int i = 0; i < Solitaire::HISTORY; i++) {
      runner.expectTrue(game.undo(), "undo ring reverts last eight actions");
    }
    runner.expectEq(1, game.count(Solitaire::WASTE), "undo ring drops ninth-oldest snapshot");
    game.reset(9);
    runner.expectFalse(game.canUndo(), "reset clears undo history");
  }

  {
    Solitaire game;
    papyrix::games::SolitaireTestAccess::clear(game);
    for (int suit = 0; suit < 4; suit++) {
      uint8_t cards[13] = {};
      for (int rank = 1; rank <= 13; rank++) {
        cards[rank - 1] = card(suit, rank);
      }
      papyrix::games::SolitaireTestAccess::setPile(game, Solitaire::FOUNDATION_FIRST + suit, cards, nullptr, 13);
    }
    runner.expectTrue(game.won(), "won detects all foundations complete");
    expectConserved(runner, game, "winning layout");
    runner.expectTrue(sizeof(Solitaire) <= 8192, "model stays within 8 KiB size limit");
  }

  runner.printSummary();
  return runner.allPassed() ? 0 : 1;
}
