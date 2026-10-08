#pragma once

#include <cstdint>

namespace papyrix::games {

struct GameModelTestAccess;

enum class Direction : uint8_t { Up, Down, Left, Right };

class Game2048 {
 public:
  static constexpr int SIZE = 4;

  void reset(uint32_t seed = 1);
  uint32_t tile(int row, int col) const;
  void setTile(int row, int col, uint32_t value);
  void clear();
  bool move(Direction dir);
  uint32_t score() const { return score_; }
  bool won() const;
  bool gameOver() const;

 private:
  uint32_t board_[SIZE][SIZE] = {};
  uint32_t score_ = 0;
  uint32_t rng_ = 1;

  uint32_t nextRandom();
  void spawnTile();
  bool canMove(Direction dir) const;
};

class Snake {
 public:
  static constexpr int WIDTH = 16;
  static constexpr int HEIGHT = 20;
  static constexpr int CAPACITY = WIDTH * HEIGHT;

  void reset(uint32_t seed = 1);
  bool turn(Direction dir);
  bool step();
  bool body(int x, int y) const;
  int foodX() const { return foodX_; }
  int foodY() const { return foodY_; }
  uint16_t score() const { return score_; }
  bool won() const { return length_ >= CAPACITY; }
  bool gameOver() const { return gameOver_; }

 private:
  friend struct GameModelTestAccess;

  uint8_t xs_[CAPACITY] = {};
  uint8_t ys_[CAPACITY] = {};
  uint16_t length_ = 0;
  uint16_t score_ = 0;
  int foodX_ = 0;
  int foodY_ = 0;
  Direction dir_ = Direction::Right;
  Direction nextDir_ = Direction::Right;
  uint32_t rng_ = 1;
  bool gameOver_ = false;
  bool turnQueued_ = false;

  uint32_t nextRandom();
  void placeFood();
  bool isReverse(Direction dir) const;
};

class FallingBlocks {
 public:
  static constexpr int WIDTH = 10;
  static constexpr int HEIGHT = 20;

  void reset(uint32_t seed = 1);
  bool move(int dx);
  bool rotate();
  bool step();
  void drop();
  bool occupied(int x, int y) const;
  bool settled(int x, int y) const;
  uint32_t score() const { return score_; }
  uint16_t lines() const { return lines_; }
  bool gameOver() const { return gameOver_; }

 private:
  friend struct GameModelTestAccess;

  bool board_[HEIGHT][WIDTH] = {};
  uint32_t rng_ = 1;
  uint32_t score_ = 0;
  uint16_t lines_ = 0;
  uint8_t piece_ = 0;
  uint8_t rotation_ = 0;
  int8_t pieceX_ = 3;
  int8_t pieceY_ = 0;
  bool gameOver_ = false;

  uint32_t nextRandom();
  void spawnPiece();
  bool pieceCell(int x, int y, uint8_t piece, uint8_t rotation) const;
  bool collides(int x, int y, uint8_t rotation) const;
  void lockPiece();
  void clearLines();
};

}  // namespace papyrix::games
