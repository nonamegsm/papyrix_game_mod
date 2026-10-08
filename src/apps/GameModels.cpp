#include "GameModels.h"

#include <algorithm>

namespace papyrix::games {

namespace {

uint32_t xorshift(uint32_t& state) {
  if (state == 0) state = 1;
  uint32_t x = state;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  state = x == 0 ? 1 : x;
  return state;
}

bool sameCell(int x, int y, int cx, int cy) { return x == cx && y == cy; }

}  // namespace

void Game2048::reset(uint32_t seed) {
  rng_ = seed == 0 ? 1 : seed;
  clear();
  spawnTile();
  spawnTile();
}

void Game2048::clear() {
  for (auto& row : board_) {
    for (uint32_t& tile : row) {
      tile = 0;
    }
  }
  score_ = 0;
}

uint32_t Game2048::tile(int row, int col) const {
  if (row < 0 || row >= SIZE || col < 0 || col >= SIZE) return 0;
  return board_[row][col];
}

void Game2048::setTile(int row, int col, uint32_t value) {
  if (row < 0 || row >= SIZE || col < 0 || col >= SIZE) return;
  board_[row][col] = value;
}

uint32_t Game2048::nextRandom() { return xorshift(rng_); }

void Game2048::spawnTile() {
  int empty = 0;
  for (int row = 0; row < SIZE; row++) {
    for (int col = 0; col < SIZE; col++) {
      if (board_[row][col] == 0) empty++;
    }
  }
  if (empty == 0) return;

  int pick = static_cast<int>(nextRandom() % static_cast<uint32_t>(empty));
  const uint32_t value = (nextRandom() % 10 == 0) ? 4 : 2;
  for (int row = 0; row < SIZE; row++) {
    for (int col = 0; col < SIZE; col++) {
      if (board_[row][col] != 0) continue;
      if (pick-- == 0) {
        board_[row][col] = value;
        return;
      }
    }
  }
}

bool Game2048::move(Direction dir) {
  bool changed = false;

  for (int line = 0; line < SIZE; line++) {
    uint32_t values[SIZE] = {};
    int count = 0;

    for (int i = 0; i < SIZE; i++) {
      int row = line;
      int col = i;
      if (dir == Direction::Right) col = SIZE - 1 - i;
      if (dir == Direction::Up || dir == Direction::Down) {
        row = (dir == Direction::Down) ? SIZE - 1 - i : i;
        col = line;
      }
      const uint32_t value = board_[row][col];
      if (value != 0) values[count++] = value;
    }

    uint32_t merged[SIZE] = {};
    int out = 0;
    for (int i = 0; i < count; i++) {
      if (i + 1 < count && values[i] == values[i + 1]) {
        merged[out] = values[i] * 2;
        score_ += merged[out];
        out++;
        i++;
      } else {
        merged[out++] = values[i];
      }
    }

    for (int i = 0; i < SIZE; i++) {
      int row = line;
      int col = i;
      if (dir == Direction::Right) col = SIZE - 1 - i;
      if (dir == Direction::Up || dir == Direction::Down) {
        row = (dir == Direction::Down) ? SIZE - 1 - i : i;
        col = line;
      }
      if (board_[row][col] != merged[i]) {
        board_[row][col] = merged[i];
        changed = true;
      }
    }
  }

  if (changed) spawnTile();
  return changed;
}

bool Game2048::won() const {
  for (const auto& row : board_) {
    for (uint32_t value : row) {
      if (value >= 2048) return true;
    }
  }
  return false;
}

bool Game2048::canMove(Direction dir) const {
  Game2048 copy = *this;
  return copy.move(dir);
}

bool Game2048::gameOver() const {
  for (const auto& row : board_) {
    for (uint32_t value : row) {
      if (value == 0) return false;
    }
  }
  return !canMove(Direction::Up) && !canMove(Direction::Down) && !canMove(Direction::Left) &&
         !canMove(Direction::Right);
}

void Snake::reset(uint32_t seed) {
  rng_ = seed == 0 ? 1 : seed;
  length_ = 3;
  score_ = 0;
  dir_ = Direction::Right;
  nextDir_ = dir_;
  turnQueued_ = false;
  gameOver_ = false;
  xs_[0] = WIDTH / 2;
  ys_[0] = HEIGHT / 2;
  xs_[1] = xs_[0] - 1;
  ys_[1] = ys_[0];
  xs_[2] = xs_[0] - 2;
  ys_[2] = ys_[0];
  placeFood();
}

uint32_t Snake::nextRandom() { return xorshift(rng_); }

bool Snake::body(int x, int y) const {
  for (uint16_t i = 0; i < length_; i++) {
    if (sameCell(x, y, xs_[i], ys_[i])) return true;
  }
  return false;
}

bool Snake::isReverse(Direction dir) const {
  return (dir_ == Direction::Up && dir == Direction::Down) || (dir_ == Direction::Down && dir == Direction::Up) ||
         (dir_ == Direction::Left && dir == Direction::Right) || (dir_ == Direction::Right && dir == Direction::Left);
}

bool Snake::turn(Direction dir) {
  if (turnQueued_ && dir != nextDir_) return false;
  if (isReverse(dir)) return false;
  nextDir_ = dir;
  turnQueued_ = true;
  return true;
}

void Snake::placeFood() {
  if (won()) return;
  const int freeCells = CAPACITY - length_;
  int pick = static_cast<int>(nextRandom() % static_cast<uint32_t>(freeCells));
  for (int y = 0; y < HEIGHT; y++) {
    for (int x = 0; x < WIDTH; x++) {
      if (body(x, y)) continue;
      if (pick-- == 0) {
        foodX_ = x;
        foodY_ = y;
        return;
      }
    }
  }
}

bool Snake::step() {
  if (gameOver_ || won()) return false;
  dir_ = nextDir_;
  turnQueued_ = false;

  int newX = xs_[0];
  int newY = ys_[0];
  if (dir_ == Direction::Up) newY--;
  if (dir_ == Direction::Down) newY++;
  if (dir_ == Direction::Left) newX--;
  if (dir_ == Direction::Right) newX++;

  const bool eats = sameCell(newX, newY, foodX_, foodY_);
  const uint16_t bodyToCheck = eats ? length_ : static_cast<uint16_t>(length_ - 1);
  if (newX < 0 || newX >= WIDTH || newY < 0 || newY >= HEIGHT) {
    gameOver_ = true;
    return true;
  }
  for (uint16_t i = 0; i < bodyToCheck; i++) {
    if (sameCell(newX, newY, xs_[i], ys_[i])) {
      gameOver_ = true;
      return true;
    }
  }

  const uint16_t newLength = eats ? static_cast<uint16_t>(length_ + 1) : length_;
  for (uint16_t i = newLength - 1; i > 0; i--) {
    xs_[i] = xs_[i - 1];
    ys_[i] = ys_[i - 1];
  }
  xs_[0] = static_cast<uint8_t>(newX);
  ys_[0] = static_cast<uint8_t>(newY);
  length_ = newLength;

  if (eats) {
    score_++;
    placeFood();
  }
  return true;
}

namespace {

constexpr uint16_t PIECES[7][4] = {
    {0x0F00, 0x2222, 0x00F0, 0x4444},  // I
    {0x8E00, 0x6440, 0x0E20, 0x44C0},  // J
    {0x2E00, 0x4460, 0x0E80, 0xC440},  // L
    {0x6600, 0x6600, 0x6600, 0x6600},  // O
    {0x6C00, 0x4620, 0x06C0, 0x8C40},  // S
    {0x4E00, 0x4640, 0x0E40, 0x4C40},  // T
    {0xC600, 0x2640, 0x0C60, 0x4C80},  // Z
};

}  // namespace

void FallingBlocks::reset(uint32_t seed) {
  rng_ = seed == 0 ? 1 : seed;
  for (auto& row : board_) {
    for (bool& cell : row) {
      cell = false;
    }
  }
  score_ = 0;
  lines_ = 0;
  gameOver_ = false;
  spawnPiece();
}

uint32_t FallingBlocks::nextRandom() { return xorshift(rng_); }

bool FallingBlocks::pieceCell(int x, int y, uint8_t piece, uint8_t rotation) const {
  const uint16_t mask = PIECES[piece % 7][rotation % 4];
  return (mask & (0x8000u >> (y * 4 + x))) != 0;
}

bool FallingBlocks::collides(int x, int y, uint8_t rotation) const {
  for (int py = 0; py < 4; py++) {
    for (int px = 0; px < 4; px++) {
      if (!pieceCell(px, py, piece_, rotation)) continue;
      const int bx = x + px;
      const int by = y + py;
      if (bx < 0 || bx >= WIDTH || by >= HEIGHT) return true;
      if (by >= 0 && board_[by][bx]) return true;
    }
  }
  return false;
}

void FallingBlocks::spawnPiece() {
  piece_ = static_cast<uint8_t>(nextRandom() % 7);
  rotation_ = 0;
  pieceX_ = 3;
  pieceY_ = 0;
  if (collides(pieceX_, pieceY_, rotation_)) gameOver_ = true;
}

bool FallingBlocks::move(int dx) {
  if (gameOver_ || dx == 0) return false;
  const int nx = pieceX_ + (dx < 0 ? -1 : 1);
  if (collides(nx, pieceY_, rotation_)) return false;
  pieceX_ = static_cast<int8_t>(nx);
  return true;
}

bool FallingBlocks::rotate() {
  if (gameOver_) return false;
  const uint8_t next = static_cast<uint8_t>((rotation_ + 1) % 4);
  if (!collides(pieceX_, pieceY_, next)) {
    rotation_ = next;
    return true;
  }
  if (!collides(pieceX_ - 1, pieceY_, next)) {
    pieceX_--;
    rotation_ = next;
    return true;
  }
  if (!collides(pieceX_ + 1, pieceY_, next)) {
    pieceX_++;
    rotation_ = next;
    return true;
  }
  return false;
}

bool FallingBlocks::step() {
  if (gameOver_) return false;
  if (!collides(pieceX_, pieceY_ + 1, rotation_)) {
    pieceY_++;
    return true;
  }
  lockPiece();
  clearLines();
  spawnPiece();
  return true;
}

void FallingBlocks::drop() {
  if (gameOver_) return;
  while (!collides(pieceX_, pieceY_ + 1, rotation_)) {
    pieceY_++;
    score_++;
  }
  lockPiece();
  clearLines();
  spawnPiece();
}

void FallingBlocks::lockPiece() {
  for (int py = 0; py < 4; py++) {
    for (int px = 0; px < 4; px++) {
      if (!pieceCell(px, py, piece_, rotation_)) continue;
      const int bx = pieceX_ + px;
      const int by = pieceY_ + py;
      if (bx >= 0 && bx < WIDTH && by >= 0 && by < HEIGHT) {
        board_[by][bx] = true;
      } else if (by < 0) {
        gameOver_ = true;
      }
    }
  }
}

void FallingBlocks::clearLines() {
  int cleared = 0;
  for (int y = HEIGHT - 1; y >= 0; y--) {
    bool full = true;
    for (int x = 0; x < WIDTH; x++) {
      if (!board_[y][x]) {
        full = false;
        break;
      }
    }
    if (!full) continue;

    cleared++;
    for (int yy = y; yy > 0; yy--) {
      for (int x = 0; x < WIDTH; x++) {
        board_[yy][x] = board_[yy - 1][x];
      }
    }
    for (int x = 0; x < WIDTH; x++) {
      board_[0][x] = false;
    }
    y++;
  }

  if (cleared > 0) {
    lines_ = static_cast<uint16_t>(lines_ + cleared);
    score_ += static_cast<uint32_t>(cleared * cleared * 100);
  }
}

bool FallingBlocks::settled(int x, int y) const {
  if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT) return false;
  return board_[y][x];
}

bool FallingBlocks::occupied(int x, int y) const {
  if (settled(x, y)) return true;
  for (int py = 0; py < 4; py++) {
    for (int px = 0; px < 4; px++) {
      if (!pieceCell(px, py, piece_, rotation_)) continue;
      if (sameCell(x, y, pieceX_ + px, pieceY_ + py)) return true;
    }
  }
  return false;
}

}  // namespace papyrix::games
