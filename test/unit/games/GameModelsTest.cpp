#include <apps/GameModels.h>

#include "test_utils.h"

using papyrix::games::Direction;
using papyrix::games::FallingBlocks;
using papyrix::games::Game2048;
using papyrix::games::Snake;

namespace papyrix::games {

struct GameModelTestAccess {
  static void setSnake(Snake& snake, const uint8_t* xs, const uint8_t* ys, uint16_t length, Direction dir, int foodX,
                       int foodY) {
    snake.length_ = length;
    snake.score_ = 0;
    snake.dir_ = dir;
    snake.nextDir_ = dir;
    snake.turnQueued_ = false;
    snake.gameOver_ = false;
    snake.foodX_ = foodX;
    snake.foodY_ = foodY;
    for (uint16_t i = 0; i < length; i++) {
      snake.xs_[i] = xs[i];
      snake.ys_[i] = ys[i];
    }
  }

  static uint16_t snakeLength(const Snake& snake) { return snake.length_; }

  static void clearBlocks(FallingBlocks& blocks) {
    for (auto& row : blocks.board_) {
      for (bool& cell : row) {
        cell = false;
      }
    }
    blocks.score_ = 0;
    blocks.lines_ = 0;
    blocks.gameOver_ = false;
  }

  static void setSettled(FallingBlocks& blocks, int x, int y, bool value) { blocks.board_[y][x] = value; }

  static void setPiece(FallingBlocks& blocks, uint8_t piece, uint8_t rotation, int8_t x, int8_t y) {
    blocks.piece_ = piece;
    blocks.rotation_ = rotation;
    blocks.pieceX_ = x;
    blocks.pieceY_ = y;
    blocks.gameOver_ = false;
  }
};

}  // namespace papyrix::games

namespace {

int count2048Tiles(const Game2048& game) {
  int count = 0;
  for (int row = 0; row < Game2048::SIZE; row++) {
    for (int col = 0; col < Game2048::SIZE; col++) {
      if (game.tile(row, col) != 0) count++;
    }
  }
  return count;
}

int countSnakeCells(const Snake& snake) {
  int count = 0;
  for (int y = 0; y < Snake::HEIGHT; y++) {
    for (int x = 0; x < Snake::WIDTH; x++) {
      if (snake.body(x, y)) count++;
    }
  }
  return count;
}

int countBlockCells(const FallingBlocks& blocks) {
  int count = 0;
  for (int y = 0; y < FallingBlocks::HEIGHT; y++) {
    for (int x = 0; x < FallingBlocks::WIDTH; x++) {
      if (blocks.occupied(x, y)) count++;
    }
  }
  return count;
}

}  // namespace

int main() {
  TestUtils::TestRunner runner("GameModelsTest");

  {
    Game2048 game;
    game.reset(42);
    runner.expectEq(2, count2048Tiles(game), "2048 reset spawns two tiles");
  }

  {
    Game2048 game;
    game.reset(1);
    game.clear();
    game.setTile(0, 0, 2);
    game.setTile(0, 1, 2);
    game.setTile(0, 2, 2);
    game.setTile(0, 3, 2);

    runner.expectTrue(game.move(Direction::Left), "2048 move reports changed board");
    runner.expectEq(uint32_t(4), game.tile(0, 0), "2048 first pair merges");
    runner.expectEq(uint32_t(4), game.tile(0, 1), "2048 second pair merges once");
    runner.expectEq(uint32_t(8), game.score(), "2048 merge score accumulates");
    runner.expectEq(3, count2048Tiles(game), "2048 valid move spawns exactly one new tile");
  }

  {
    Game2048 game;
    game.reset(1);
    game.clear();
    game.setTile(0, 0, 2);
    runner.expectFalse(game.move(Direction::Left), "2048 blocked move does not spawn");
    runner.expectEq(1, count2048Tiles(game), "2048 invalid move keeps tile count");
  }

  {
    Game2048 game;
    game.clear();
    const uint16_t values[4][4] = {{2, 4, 2, 4}, {4, 2, 4, 2}, {2, 4, 2, 4}, {4, 2, 4, 2}};
    for (int row = 0; row < Game2048::SIZE; row++) {
      for (int col = 0; col < Game2048::SIZE; col++) {
        game.setTile(row, col, values[row][col]);
      }
    }
    runner.expectTrue(game.gameOver(), "2048 detects full board without moves");
    game.setTile(3, 3, 2048);
    runner.expectTrue(game.won(), "2048 detects winning tile");
  }

  {
    Game2048 game;
    game.clear();
    game.setTile(0, 0, 32768);
    game.setTile(0, 1, 32768);
    runner.expectTrue(game.move(Direction::Left), "2048 merges above 16-bit range");
    runner.expectEq(uint32_t(65536), game.tile(0, 0), "2048 tile values use 32-bit storage");
  }

  {
    Game2048 game;
    game.clear();
    const uint32_t values[4][4] = {{65536, 4, 2, 4}, {4, 2, 4, 2}, {2, 4, 2, 4}, {4, 2, 4, 2}};
    for (int row = 0; row < Game2048::SIZE; row++) {
      for (int col = 0; col < Game2048::SIZE; col++) {
        game.setTile(row, col, values[row][col]);
      }
    }
    runner.expectTrue(game.gameOver(), "2048 gameOver handles 32-bit tile values");
  }

  {
    Snake snake;
    snake.reset(7);
    runner.expectEq(3, countSnakeCells(snake), "snake reset creates length three body");
    runner.expectFalse(snake.body(snake.foodX(), snake.foodY()), "snake food is not placed on body");
    runner.expectFalse(snake.turn(Direction::Left), "snake reports rejected immediate reverse");
    snake.step();
    runner.expectFalse(snake.gameOver(), "snake rejects immediate reverse direction");
  }

  {
    Snake snake;
    snake.reset(8);
    runner.expectTrue(snake.turn(Direction::Up), "snake accepts first turn before step");
    runner.expectFalse(snake.turn(Direction::Left), "snake rejects second different turn before step");
    snake.step();
    runner.expectFalse(snake.gameOver(), "snake ignores second turn before the next step");
  }

  {
    Snake snake;
    snake.reset(10);
    if (snake.turn(Direction::Left)) {
      snake.step();
    }
    runner.expectFalse(snake.gameOver(), "snake rejected reverse can guard manual stepping");
  }

  {
    Snake snake;
    snake.reset(1);
    const uint8_t xs[] = {2, 1, 0};
    const uint8_t ys[] = {1, 1, 1};
    papyrix::games::GameModelTestAccess::setSnake(snake, xs, ys, 3, Direction::Right, 10, 10);
    runner.expectTrue(snake.step(), "snake advances when next cell is empty");
    runner.expectFalse(snake.body(0, 1), "snake vacates tail cell when not eating");
    runner.expectTrue(snake.body(3, 1), "snake moves head into next cell");
  }

  {
    Snake snake;
    snake.reset(1);
    const uint8_t xs[] = {2, 1, 0};
    const uint8_t ys[] = {1, 1, 1};
    papyrix::games::GameModelTestAccess::setSnake(snake, xs, ys, 3, Direction::Right, 3, 1);
    runner.expectTrue(snake.step(), "snake advances into food cell");
    runner.expectEq(uint16_t(4), papyrix::games::GameModelTestAccess::snakeLength(snake), "snake grows after eating");
    runner.expectEq(uint16_t(1), snake.score(), "snake score increments after eating");
  }

  {
    Snake snake;
    snake.reset(1);
    const uint8_t xs[] = {2, 2, 1, 1, 1};
    const uint8_t ys[] = {2, 3, 3, 2, 1};
    papyrix::games::GameModelTestAccess::setSnake(snake, xs, ys, 5, Direction::Left, 10, 10);
    runner.expectTrue(snake.step(), "snake step records self-collision state change");
    runner.expectTrue(snake.gameOver(), "snake collides with its own body");
  }

  {
    Snake snake;
    snake.reset(9);
    for (int i = 0; i < Snake::WIDTH; i++) {
      snake.step();
    }
    runner.expectTrue(snake.gameOver(), "snake hits wall");
  }

  {
    FallingBlocks blocks;
    blocks.reset(11);
    runner.expectEq(4, countBlockCells(blocks), "blocks reset spawns four-cell piece");
    runner.expectTrue(blocks.move(-1), "blocks piece moves left when space exists");
    runner.expectTrue(blocks.rotate(), "blocks piece rotates when space exists");
  }

  {
    FallingBlocks blocks;
    blocks.reset(13);
    blocks.drop();
    runner.expectTrue(blocks.score() > 0, "blocks hard drop adds score");
    runner.expectTrue(countBlockCells(blocks) >= 4, "blocks hard drop locks and respawns piece");
  }

  {
    FallingBlocks blocks;
    blocks.reset(1);
    papyrix::games::GameModelTestAccess::clearBlocks(blocks);
    papyrix::games::GameModelTestAccess::setPiece(blocks, 0, 0, 0, 0);
    runner.expectFalse(blocks.move(-1), "blocks cannot move through left wall");
  }

  {
    FallingBlocks blocks;
    blocks.reset(1);
    papyrix::games::GameModelTestAccess::clearBlocks(blocks);
    papyrix::games::GameModelTestAccess::setPiece(blocks, 5, 0, 3, 0);
    for (int y = 0; y < 4; y++) {
      for (int x = 0; x < FallingBlocks::WIDTH; x++) {
        papyrix::games::GameModelTestAccess::setSettled(blocks, x, y, true);
      }
    }
    papyrix::games::GameModelTestAccess::setSettled(blocks, 4, 0, false);
    papyrix::games::GameModelTestAccess::setSettled(blocks, 3, 1, false);
    papyrix::games::GameModelTestAccess::setSettled(blocks, 4, 1, false);
    papyrix::games::GameModelTestAccess::setSettled(blocks, 5, 1, false);
    runner.expectFalse(blocks.rotate(), "blocks rotation fails when all rotation positions are blocked");
  }

  {
    FallingBlocks blocks;
    blocks.reset(1);
    papyrix::games::GameModelTestAccess::clearBlocks(blocks);
    for (int x = 0; x < FallingBlocks::WIDTH; x++) {
      if (x == 4 || x == 5) continue;
      papyrix::games::GameModelTestAccess::setSettled(blocks, x, 18, true);
      papyrix::games::GameModelTestAccess::setSettled(blocks, x, 19, true);
    }
    papyrix::games::GameModelTestAccess::setPiece(blocks, 3, 0, 3, 18);
    runner.expectTrue(blocks.step(), "blocks lock piece and clear completed lines");
    runner.expectEq(uint16_t(2), blocks.lines(), "blocks clears adjacent completed lines");
    runner.expectEq(uint32_t(400), blocks.score(), "blocks scores multi-line clear");
    runner.expectFalse(blocks.settled(4, 19), "blocks compacts cleared rows away");
  }

  runner.printSummary();
  return runner.allPassed() ? 0 : 1;
}
