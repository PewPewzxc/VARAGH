#include "TicTacToeGame.h"

#include <cstring>

namespace tictactoe {

namespace {

constexpr uint8_t kLines[8][3] = {{0, 1, 2}, {3, 4, 5}, {6, 7, 8}, {0, 3, 6}, {1, 4, 7}, {2, 5, 8}, {0, 4, 8}, {2, 4, 6}};
constexpr int kWin = 100;
constexpr int kInfinity = 1000;
constexpr uint8_t kInfiniteMarks = 6;  // three a side

}  // namespace

void Game::start(const Kind kind) {
  *this = Game{};
  kind_ = kind;
  state_ = State::Playing;
}

int Game::fading() const {
  return kind_ == Kind::Infinite && state_ == State::Playing && alive_ == kInfiniteMarks ? order_[0] : -1;
}

bool Game::winningCells(int cells[3]) const {
  // Only the side that has just moved can have completed a row.
  const Mark last = other(toMove_);
  for (const auto& line : kLines) {
    if (board_[line[0]] != last || board_[line[1]] != last || board_[line[2]] != last) continue;
    for (int i = 0; i < 3; ++i) cells[i] = line[i];
    return true;
  }
  return false;
}

bool Game::play(const int cell) {
  if (state_ != State::Playing || cell < 0 || cell > 8 || board_[cell] != None) return false;
  if (kind_ == Kind::Infinite && alive_ == kInfiniteMarks) {
    // Marks alternate, so with three a side the oldest one is the mover's.
    board_[order_[0]] = None;
    memmove(order_, order_ + 1, --alive_);
  }
  order_[alive_++] = static_cast<uint8_t>(cell);
  board_[cell] = toMove_;
  toMove_ = other(toMove_);
  int cells[3];
  if (winningCells(cells)) {
    state_ = toMove_ == O ? State::XWins : State::OWins;
  } else if (alive_ == 9) {
    state_ = State::Draw;
  }
  return true;
}

// The value of the position for the side to move; a row sooner counts for more.
int Game::search(const int depth, int alpha, const int beta) const {
  if (state_ == State::Draw) return 0;
  if (state_ != State::Playing) return -(kWin + depth);
  if (depth == 0) return 0;
  int best = -kInfinity;
  for (int cell = 0; cell < 9; ++cell) {
    Game next = *this;
    if (!next.play(cell)) continue;
    const int score = -next.search(depth - 1, -beta, -alpha);
    if (score > best) best = score;
    if (best > alpha) alpha = best;
    if (alpha >= beta) break;
  }
  return best;
}

int Game::bestMove(const int depth, uint32_t seed) const {
  int best = -kInfinity;
  int chosen = -1;
  uint32_t equals = 0;
  for (int cell = 0; cell < 9; ++cell) {
    Game next = *this;
    if (!next.play(cell)) continue;
    // The whole window, so that equally good cells really score the same.
    const int score = -next.search(depth - 1, -kInfinity, kInfinity);
    if (score > best) {
      best = score;
      chosen = cell;
      equals = 1;
    } else if (score == best) {
      seed = seed * 1664525U + 1013904223U;
      if ((seed >> 16) % ++equals == 0) chosen = cell;
    }
  }
  return chosen;
}

int Game::depthForLevel(const int level, const Kind kind) {
  if (level <= 0) return 1;  // takes a row it can complete, nothing more
  if (level == 1) return 2;  // also stops the other side's row
  return kind == Kind::Classic ? 9 : 8;
}

void Game::save(uint8_t* out) const {
  out[0] = static_cast<uint8_t>(kind_);
  out[1] = static_cast<uint8_t>(toMove_) | (started() ? 0x80 : 0);
  out[2] = alive_;
  memcpy(out + 3, order_, sizeof(order_));
}

bool Game::load(const uint8_t* in, const size_t size) {
  if (size < kSaveBytes) return false;
  const uint8_t alive = in[2];
  const Mark mover = static_cast<Mark>(in[1] & 0x7F);
  if (in[0] > 1 || (mover != X && mover != O) || alive > (in[0] == 1 ? kInfiniteMarks : 9)) return false;
  Game loaded;
  loaded.kind_ = static_cast<Kind>(in[0]);
  loaded.toMove_ = mover;
  loaded.alive_ = alive;
  for (int i = 0; i < alive; ++i) {
    const uint8_t cell = in[3 + i];
    if (cell > 8 || loaded.board_[cell] != None) return false;
    loaded.order_[i] = cell;
    // The newest mark is the last mover's, and they alternate back from there.
    loaded.board_[cell] = ((alive - 1 - i) & 1) ? mover : other(mover);
  }
  if (in[1] & 0x80) {
    int cells[3];
    loaded.state_ = loaded.winningCells(cells)
                        ? (mover == O ? State::XWins : State::OWins)
                        : (alive == 9 ? State::Draw : State::Playing);
  }
  *this = loaded;
  return true;
}

}  // namespace tictactoe
