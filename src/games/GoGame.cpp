#include "GoGame.h"

#include <cstring>

namespace go {

namespace {

// Marks for the flood fills: 0 unseen, then a value per kind of point.
// Small on purpose: these fills run on the main task's stack too.
struct Marks {
  uint8_t at[kMaxPoints] = {};
  uint8_t generation = 0;
  uint8_t next() { return ++generation; }
};

uint32_t nextRandom(uint32_t& state) {
  state ^= state << 13;
  state ^= state >> 17;
  state ^= state << 5;
  return state;
}

}  // namespace

void Board::reset(const uint8_t boardSize) {
  size = boardSize == 13 || boardSize == 19 ? boardSize : 9;
  memset(cell, 0, sizeof(cell));
  toMove = Black;
  ko = -1;
  captured[0] = captured[1] = captured[2] = 0;
}

int Board::neighbours(const int point, int out[4]) const {
  const int row = point / size;
  const int column = point % size;
  int count = 0;
  if (row > 0) out[count++] = point - size;
  if (row < size - 1) out[count++] = point + size;
  if (column > 0) out[count++] = point - 1;
  if (column < size - 1) out[count++] = point + 1;
  return count;
}

int Board::liberties(const int point, int16_t* stones, int* stoneCount) const {
  const uint8_t colour = cell[point];
  if (colour == Empty) return 0;
  Marks marks;
  const uint8_t seen = marks.next();
  int16_t stack[kMaxPoints];
  int top = 0;
  int found = 0;
  int count = 0;
  stack[top++] = static_cast<int16_t>(point);
  marks.at[point] = seen;
  // Liberties get their own mark value so each is counted once.
  const uint8_t liberty = marks.next();
  while (top > 0) {
    const int p = stack[--top];
    if (stones) stones[count] = static_cast<int16_t>(p);
    ++count;
    int around[4];
    const int n = neighbours(p, around);
    for (int i = 0; i < n; ++i) {
      const int q = around[i];
      if (cell[q] == Empty) {
        if (marks.at[q] != liberty) {
          marks.at[q] = liberty;
          ++found;
        }
      } else if (cell[q] == colour && marks.at[q] != seen) {
        marks.at[q] = seen;
        stack[top++] = static_cast<int16_t>(q);
      }
    }
  }
  if (stoneCount) *stoneCount = count;
  return found;
}

bool Board::legal(const int point) const {
  if (point < 0 || point >= points() || cell[point] != Empty || point == ko) return false;
  Board copy = *this;
  return copy.play(point);
}

bool Board::play(const int point) {
  if (point < 0 || point >= points() || cell[point] != Empty || point == ko) return false;
  const uint8_t me = toMove;
  const uint8_t them = other(static_cast<Stone>(me));
  uint8_t before[kMaxPoints];
  memcpy(before, cell, points());
  cell[point] = me;

  int taken = 0;
  int lastTaken = -1;
  int around[4];
  const int n = neighbours(point, around);
  for (int i = 0; i < n; ++i) {
    const int q = around[i];
    if (cell[q] != them) continue;
    int16_t chain[kMaxPoints];
    int chainSize = 0;
    if (liberties(q, chain, &chainSize) != 0) continue;
    for (int s = 0; s < chainSize; ++s) cell[chain[s]] = Empty;
    taken += chainSize;
    lastTaken = chain[0];
  }
  int ownSize = 0;
  const int ownLiberties = liberties(point, nullptr, &ownSize);
  if (ownLiberties == 0) {
    memcpy(cell, before, points());  // suicide
    return false;
  }
  captured[me] = static_cast<uint16_t>(captured[me] + taken);
  // A lone stone that took a lone stone and now has that point as its only
  // liberty may not be taken straight back.
  ko = (taken == 1 && ownSize == 1 && ownLiberties == 1) ? static_cast<int16_t>(lastTaken) : -1;
  toMove = them;
  return true;
}

void Board::pass() {
  ko = -1;
  toMove = other(static_cast<Stone>(toMove));
}

Score score(const Board& board, const uint8_t* deadBits, const int komiTenths) {
  Board area = board;
  const auto dead = [deadBits](const int p) { return deadBits && ((deadBits[p >> 3] >> (p & 7)) & 1); };
  for (int p = 0; p < area.points(); ++p) {
    if (area.cell[p] != Empty && dead(p)) area.cell[p] = Empty;
  }
  Score result;
  Marks marks;
  const uint8_t seen = marks.next();
  int16_t stack[kMaxPoints];
  for (int p = 0; p < area.points(); ++p) {
    if (area.cell[p] == Black) result.blackTenths += 10;
    if (area.cell[p] == White) result.whiteTenths += 10;
    if (area.cell[p] != Empty || marks.at[p] == seen) continue;
    // An empty region belongs to the colour that alone borders it.
    int top = 0;
    int regionSize = 0;
    bool touchesBlack = false;
    bool touchesWhite = false;
    stack[top++] = static_cast<int16_t>(p);
    marks.at[p] = seen;
    while (top > 0) {
      const int q = stack[--top];
      ++regionSize;
      int around[4];
      const int n = area.neighbours(q, around);
      for (int i = 0; i < n; ++i) {
        const int r = around[i];
        if (area.cell[r] == Black) {
          touchesBlack = true;
        } else if (area.cell[r] == White) {
          touchesWhite = true;
        } else if (marks.at[r] != seen) {
          marks.at[r] = seen;
          stack[top++] = static_cast<int16_t>(r);
        }
      }
    }
    if (touchesBlack && !touchesWhite) result.blackTenths += regionSize * 10;
    if (touchesWhite && !touchesBlack) result.whiteTenths += regionSize * 10;
  }
  result.whiteTenths += komiTenths;
  return result;
}

int chooseMove(const Board& board, const int level, const bool opponentPassed, uint32_t seed) {
  if (seed == 0) seed = 1;
  const uint8_t me = board.toMove;
  const uint8_t them = other(static_cast<Stone>(me));
  const int size = board.size;
  int stonesOnBoard = 0;
  for (int p = 0; p < board.points(); ++p) stonesOnBoard += board.cell[p] != Empty;
  const bool opening = stonesOnBoard < size * 2;

  int best = kPass;
  int bestScore = -1000000;
  for (int p = 0; p < board.points(); ++p) {
    if (board.cell[p] != Empty || p == board.ko) continue;
    int around[4];
    const int n = board.neighbours(p, around);
    // Never fill an own eye: every neighbour is an own stone with breath to spare.
    bool ownEye = true;
    for (int i = 0; i < n && ownEye; ++i) {
      ownEye = board.cell[around[i]] == me && board.liberties(around[i]) > 1;
    }
    if (ownEye) continue;

    // Chains next to the point before the move.
    int ownInAtari = 0;
    int theirLibertiesBefore[4] = {0, 0, 0, 0};
    for (int i = 0; i < n; ++i) {
      const int q = around[i];
      if (board.cell[q] == me && board.liberties(q) == 1) {
        int chain = 0;
        board.liberties(q, nullptr, &chain);
        ownInAtari += chain;
      }
      if (board.cell[q] == them) theirLibertiesBefore[i] = board.liberties(q);
    }

    Board after = board;
    if (!after.play(p)) continue;
    int value = 0;
    value += (after.captured[me] - board.captured[me]) * 120;
    int ownSize = 0;
    const int ownLiberties = after.liberties(p, nullptr, &ownSize);
    if (ownLiberties == 1) value -= 90 * ownSize;      // walks into atari
    if (ownInAtari > 0 && ownLiberties >= 2) value += 70 * ownInAtari;  // saves a chain
    value += ownLiberties * 3;
    for (int i = 0; i < n; ++i) {
      const int q = around[i];
      if (after.cell[q] != them) continue;
      const int libs = after.liberties(q);
      if (libs == 1 && theirLibertiesBefore[i] > 1) {
        int chain = 0;
        after.liberties(q, nullptr, &chain);
        value += 35 * chain;  // puts a chain in atari
      }
      if (libs < theirLibertiesBefore[i]) value += 4;
    }
    // Lines from the edge: the 3rd and 4th lines first, the edge last.
    const int row = p / size;
    const int column = p % size;
    int edge = row < column ? row : column;
    if (size - 1 - row < edge) edge = size - 1 - row;
    if (size - 1 - column < edge) edge = size - 1 - column;
    const int line = edge + 1;
    if (line == 1) value -= opening ? 20 : 6;
    if (opening && (line == 3 || line == 4)) value += 12;
    // Contact with stones keeps the play alive in the middle game.
    for (int i = 0; i < n; ++i) {
      if (board.cell[around[i]] != Empty) value += 2;
    }
    const int noise = level == 0 ? 60 : 8;
    value += static_cast<int>(nextRandom(seed) % static_cast<uint32_t>(noise));
    if (value > bestScore) {
      bestScore = value;
      best = p;
    }
  }
  // Pass back once nothing on the board is worth a move any more.
  if (best != kPass && opponentPassed && bestScore < 60) return kPass;
  if (best != kPass && bestScore < -150) return kPass;
  return best;
}

void Game::start(const uint8_t size) {
  board_.reset(size);
  moveCount_ = 0;
  memset(dead_, 0, sizeof(dead_));
  state_ = State::Playing;
}

bool Game::play(const int point) {
  if (state_ != State::Playing || moveCount_ >= kMaxMoves) return false;
  if (!board_.play(point)) return false;
  moves_[moveCount_++] = static_cast<int16_t>(point);
  return true;
}

void Game::pass() {
  if (state_ != State::Playing) return;
  if (moveCount_ >= kMaxMoves) {
    memset(dead_, 0, sizeof(dead_));
    state_ = State::MarkingDead;
    return;
  }
  const bool second = lastWasPass();
  board_.pass();
  moves_[moveCount_++] = kPass;
  if (second) {
    memset(dead_, 0, sizeof(dead_));
    state_ = State::MarkingDead;
  }
}

void Game::toggleDead(const int point) {
  if (state_ != State::MarkingDead || point < 0 || point >= board_.points() || board_.cell[point] == Empty) return;
  int16_t chain[kMaxPoints];
  int count = 0;
  board_.liberties(point, chain, &count);
  const bool mark = !isDead(point);
  for (int i = 0; i < count; ++i) {
    const int p = chain[i];
    if (mark) {
      dead_[p >> 3] = static_cast<uint8_t>(dead_[p >> 3] | (1 << (p & 7)));
    } else {
      dead_[p >> 3] = static_cast<uint8_t>(dead_[p >> 3] & ~(1 << (p & 7)));
    }
  }
}

void Game::resume() {
  if (state_ != State::MarkingDead && state_ != State::Finished) return;
  memset(dead_, 0, sizeof(dead_));
  // Take back the two passes that ended the play.
  int count = moveCount_;
  while (count > 0 && moves_[count - 1] == kPass) --count;
  state_ = State::Playing;
  replay(count);
}

void Game::replay(const int count) {
  const uint8_t size = board_.size;
  board_.reset(size);
  moveCount_ = 0;
  for (int i = 0; i < count; ++i) {
    const int move = moves_[i];
    if (move == kPass) {
      board_.pass();
    } else {
      board_.play(move);
    }
    ++moveCount_;
  }
}

bool Game::undo() {
  if (moveCount_ == 0) return false;
  if (state_ != State::Playing) {
    resume();
    return true;
  }
  replay(moveCount_ - 1);
  return true;
}

size_t Game::save(uint8_t out[kSaveBytes]) const {
  memset(out, 0, kSaveBytes);
  out[0] = board_.size;
  out[1] = static_cast<uint8_t>(state_);
  out[2] = static_cast<uint8_t>(moveCount_ & 0xFF);
  out[3] = static_cast<uint8_t>(moveCount_ >> 8);
  memcpy(out + 4, dead_, kDeadBytes);
  for (int i = 0; i < moveCount_; ++i) {
    const auto move = static_cast<uint16_t>(moves_[i]);
    out[4 + kDeadBytes + i * 2] = static_cast<uint8_t>(move & 0xFF);
    out[4 + kDeadBytes + i * 2 + 1] = static_cast<uint8_t>(move >> 8);
  }
  return kSaveBytes;
}

bool Game::load(const uint8_t* data, const size_t size) {
  if (size < kSaveBytes) return false;
  const uint8_t boardSize = data[0];
  const auto state = static_cast<State>(data[1]);
  const int count = data[2] | (data[3] << 8);
  if ((boardSize != 9 && boardSize != 13 && boardSize != 19) || data[1] > static_cast<uint8_t>(State::Finished) ||
      count > kMaxMoves) {
    return false;
  }
  board_.reset(boardSize);
  state_ = State::Playing;
  for (int i = 0; i < count; ++i) {
    moves_[i] = static_cast<int16_t>(data[4 + kDeadBytes + i * 2] | (data[4 + kDeadBytes + i * 2 + 1] << 8));
  }
  replay(count);
  if (moveCount_ != count) return false;
  state_ = state;
  memcpy(dead_, data + 4, kDeadBytes);
  return true;
}

}  // namespace go
