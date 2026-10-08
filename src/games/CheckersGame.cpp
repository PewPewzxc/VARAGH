#include "CheckersGame.h"

#include <cstring>

namespace checkers {

namespace {

constexpr int kMaxPly = 32;
constexpr int kInfinity = 32000;
// Forty moves by each side without a capture or a move by an uncrowned piece.
constexpr int kQuietLimit = 80;

constexpr int8_t kRowSteps[4] = {1, 1, -1, -1};
constexpr int8_t kColSteps[4] = {-1, 1, -1, 1};

constexpr bool inside(const int row, const int col) { return row >= 0 && row < 8 && col >= 0 && col < 8; }

// Directions a piece may go: the first two entries of the tables above lead
// down the board (Dark's way), the last two up (Light's way).
void directions(const uint8_t piece, int& first, int& last) {
  if (isKing(piece)) {
    first = 0;
    last = 4;
  } else if (isLight(piece)) {
    first = 2;
    last = 4;
  } else {
    first = 0;
    last = 2;
  }
}

int jumpsFrom(const Position& p, const int square, Step* out) {
  const uint8_t piece = p.board[square];
  int first = 0;
  int last = 0;
  directions(piece, first, last);
  const int row = square >> 3;
  const int col = square & 7;
  int count = 0;
  for (int d = first; d < last; ++d) {
    const int overRow = row + kRowSteps[d];
    const int overCol = col + kColSteps[d];
    const int toRow = overRow + kRowSteps[d];
    const int toCol = overCol + kColSteps[d];
    if (!inside(toRow, toCol)) continue;
    const uint8_t over = p.board[overRow * 8 + overCol];
    if (over == Empty || sideOf(over) == sideOf(piece) || p.board[toRow * 8 + toCol] != Empty) continue;
    if (out) {
      out[count].from = static_cast<uint8_t>(square);
      out[count].to = static_cast<uint8_t>(toRow * 8 + toCol);
      out[count].captured = static_cast<uint8_t>(overRow * 8 + overCol);
    }
    ++count;
  }
  return count;
}

int slidesFrom(const Position& p, const int square, Step* out) {
  const uint8_t piece = p.board[square];
  int first = 0;
  int last = 0;
  directions(piece, first, last);
  const int row = square >> 3;
  const int col = square & 7;
  int count = 0;
  for (int d = first; d < last; ++d) {
    const int toRow = row + kRowSteps[d];
    const int toCol = col + kColSteps[d];
    if (!inside(toRow, toCol) || p.board[toRow * 8 + toCol] != Empty) continue;
    out[count].from = static_cast<uint8_t>(square);
    out[count].to = static_cast<uint8_t>(toRow * 8 + toCol);
    out[count].captured = kNone;
    ++count;
  }
  return count;
}

struct Searcher {
  StopFn stop = nullptr;
  void* context = nullptr;
  uint32_t nodes = 0;
  uint32_t maxNodes = 0;
  bool stopped = false;
  // How often a slide from one square to another refuted a line; such slides
  // are tried first everywhere else. Captures are forced, so need no order.
  uint16_t history[64][64] = {};

  void poll() {
    if (maxNodes && nodes >= maxNodes) stopped = true;
    if ((nodes & 1023) == 0 && stop && stop(context)) stopped = true;
  }

  void sortByHistory(Step* steps, const int count) const {
    for (int i = 1; i < count; ++i) {
      const Step step = steps[i];
      const uint16_t value = history[step.from][step.to];
      int j = i - 1;
      while (j >= 0 && history[steps[j].from][steps[j].to] < value) {
        steps[j + 1] = steps[j];
        --j;
      }
      steps[j + 1] = step;
    }
  }

  // Value of `step` for the side that plays it. A multi-jump keeps the turn,
  // so its next step is searched for the same side and with the same depth.
  int after(const Position& p, const Step& step, const int depth, const int alpha, const int beta, const int ply) {
    Position child = p;
    applyStep(child, step);
    if (child.side == p.side) return negamax(child, depth, alpha, beta, ply + 1);
    // A capture does not use up depth, so an exchange is always played out.
    const int next = step.captured != kNone ? depth : depth - 1;
    return -negamax(child, next, -beta, -alpha, ply + 1);
  }

  int negamax(const Position& p, const int depth, int alpha, const int beta, const int ply) {
    ++nodes;
    poll();
    if (stopped) return 0;
    Step steps[kMaxSteps];
    const int count = generateSteps(p, steps);
    if (count == 0) return -kWinScore + ply;
    if (p.quiet >= kQuietLimit) return 0;
    const bool capturing = steps[0].captured != kNone;
    if ((depth <= 0 && !capturing) || ply >= kMaxPly) return evaluate(p);
    if (!capturing) sortByHistory(steps, count);
    for (int i = 0; i < count; ++i) {
      const int score = after(p, steps[i], depth, alpha, beta, ply);
      if (stopped) return 0;
      if (score >= beta) {
        if (!capturing) {
          uint16_t& seenGood = history[steps[i].from][steps[i].to];
          if (seenGood < 60000) seenGood = static_cast<uint16_t>(seenGood + depth * depth);
        }
        return beta;
      }
      if (score > alpha) alpha = score;
    }
    return alpha;
  }
};

uint32_t nextRandom(uint32_t& state) {
  state ^= state << 13;
  state ^= state >> 17;
  state ^= state << 5;
  return state;
}

constexpr uint8_t kSaveMagic[4] = {'V', 'C', 'K', '1'};

void putU32(uint8_t* out, const uint32_t value) {
  out[0] = static_cast<uint8_t>(value);
  out[1] = static_cast<uint8_t>(value >> 8);
  out[2] = static_cast<uint8_t>(value >> 16);
  out[3] = static_cast<uint8_t>(value >> 24);
}

uint32_t getU32(const uint8_t* in) {
  return static_cast<uint32_t>(in[0]) | (static_cast<uint32_t>(in[1]) << 8) | (static_cast<uint32_t>(in[2]) << 16) |
         (static_cast<uint32_t>(in[3]) << 24);
}

// The 32 playable squares, numbered row by row.
constexpr int squareOf(const int index) { return (index / 4) * 8 + (index % 4) * 2 + (((index / 4) & 1) ? 0 : 1); }

}  // namespace

void Position::setStart() {
  memset(board, Empty, sizeof(board));
  for (int square = 0; square < 64; ++square) {
    if (!playable(square)) continue;
    const int row = square >> 3;
    if (row < 3) board[square] = Man;
    if (row > 4) board[square] = Man | kLightBit;
  }
  side = Dark;
  jumper = kNone;
  quiet = 0;
}

int Position::count(const Side who) const {
  int total = 0;
  for (const uint8_t piece : board) {
    if (piece != Empty && sideOf(piece) == who) ++total;
  }
  return total;
}

bool Position::operator==(const Position& other) const {
  return side == other.side && jumper == other.jumper && memcmp(board, other.board, sizeof(board)) == 0;
}

int generateSteps(const Position& p, Step* out) {
  if (p.jumper != kNone) return jumpsFrom(p, p.jumper, out);
  int count = 0;
  for (int square = 0; square < 64; ++square) {
    const uint8_t piece = p.board[square];
    if (piece != Empty && sideOf(piece) == p.side) count += jumpsFrom(p, square, out + count);
  }
  if (count > 0) return count;
  for (int square = 0; square < 64; ++square) {
    const uint8_t piece = p.board[square];
    if (piece != Empty && sideOf(piece) == p.side) count += slidesFrom(p, square, out + count);
  }
  return count;
}

void applyStep(Position& p, const Step& step) {
  uint8_t piece = p.board[step.from];
  const bool man = !isKing(piece);
  const bool captured = step.captured != kNone;
  bool crowned = false;
  if (man) {
    const int row = step.to >> 3;
    if (row == (isLight(piece) ? 0 : 7)) {
      piece = static_cast<uint8_t>(King | (piece & kLightBit));
      crowned = true;
    }
  }
  p.board[step.from] = Empty;
  p.board[step.to] = piece;
  if (captured) p.board[step.captured] = Empty;
  p.quiet = (captured || man) ? 0 : static_cast<uint8_t>(p.quiet < 250 ? p.quiet + 1 : p.quiet);
  // Being crowned ends the move, even where another jump would be on.
  if (captured && !crowned && jumpsFrom(p, step.to, nullptr) > 0) {
    p.jumper = step.to;
  } else {
    p.jumper = kNone;
    p.side ^= 1;
  }
}

int evaluate(const Position& p) {
  int score[2] = {0, 0};
  int pieces = 0;
  for (int square = 0; square < 64; ++square) {
    const uint8_t piece = p.board[square];
    if (piece == Empty) continue;
    ++pieces;
    const Side side = sideOf(piece);
    const int row = square >> 3;
    const int col = square & 7;
    int value = 0;
    if (isKing(piece)) {
      value = 160;
      // A king does most in the middle of the board.
      if (row >= 2 && row <= 5 && col >= 2 && col <= 5) value += 6;
    } else {
      const int advanced = side == Dark ? row : 7 - row;
      value = 100 + advanced * 3;
      // Pieces left on the home row keep the opponent from being crowned.
      if (advanced == 0) value += 8;
      if (col >= 2 && col <= 5) value += 2;
    }
    score[side] += value;
  }
  int diff = score[p.side] - score[p.side ^ 1];
  // The side that is ahead gains from every exchange.
  if (pieces > 0) diff += diff * 4 / pieces;
  return diff;
}

SearchResult search(const Position& root, const SearchOptions& options, const StopFn stop, void* context) {
  SearchResult result;
  // The history table makes this too large for a task stack.
  auto* searcher = new Searcher();
  Searcher& s = *searcher;
  s.stop = stop;
  s.context = context;
  s.maxNodes = options.maxNodes;
  Step steps[kMaxSteps];
  int scores[kMaxSteps];
  const int count = generateSteps(root, steps);
  if (count > 0) {
    result.best = steps[0];
    result.found = true;
  }
  if (count <= 1) {  // nothing to choose
    delete searcher;
    return result;
  }

  uint32_t random = options.seed ? options.seed : 1;
  const int margin = options.margin > 0 ? options.margin : 0;
  const int maxDepth = options.maxDepth < 1 ? 1 : (options.maxDepth > kMaxPly - 8 ? kMaxPly - 8 : options.maxDepth);
  for (int depth = 1; depth <= maxDepth; ++depth) {
    int best = -kInfinity;
    int bestIndex = 0;
    bool complete = true;
    for (int i = 0; i < count; ++i) {
      const int alpha = best == -kInfinity ? -kInfinity : best - margin - 1;
      const int score = s.after(root, steps[i], depth, alpha, kInfinity, 0);
      if (s.stopped) {
        complete = false;
        break;
      }
      scores[i] = score;
      if (score > best) {
        best = score;
        bestIndex = i;
      }
    }
    if (!complete) break;

    int chosen = bestIndex;
    if (margin > 0 && best < kWinScore - 1000) {
      int candidates = 0;
      for (int i = 0; i < count; ++i) {
        if (scores[i] >= best - margin) ++candidates;
      }
      int pick = static_cast<int>(nextRandom(random) % static_cast<uint32_t>(candidates));
      for (int i = 0; i < count; ++i) {
        if (scores[i] >= best - margin && pick-- == 0) chosen = i;
      }
    }
    result.best = steps[chosen];
    result.score = best;
    result.depth = depth;

    // Look at the best step first on the next, deeper pass.
    const Step bestStep = steps[bestIndex];
    for (int i = bestIndex; i > 0; --i) steps[i] = steps[i - 1];
    steps[0] = bestStep;
    if (best > kWinScore - 1000 || best < -kWinScore + 1000) break;
  }
  result.nodes = s.nodes;
  delete searcher;
  return result;
}

// --- game ---------------------------------------------------------------------------------------
Game::Snapshot Game::pack(const Position& position) {
  Snapshot snapshot;
  for (int index = 0; index < 32; ++index) {
    const uint8_t piece = position.board[squareOf(index)];
    if (piece == Empty) continue;
    (isKing(piece) ? snapshot.kings : snapshot.men)[sideOf(piece)] |= 1U << index;
  }
  snapshot.side = position.side;
  snapshot.jumper = position.jumper;
  snapshot.quiet = position.quiet;
  return snapshot;
}

void Game::unpack(const Snapshot& snapshot, Position& position) {
  memset(position.board, Empty, sizeof(position.board));
  for (int index = 0; index < 32; ++index) {
    const uint32_t bit = 1U << index;
    for (int side = 0; side < 2; ++side) {
      const uint8_t colour = side == Light ? kLightBit : 0;
      if (snapshot.men[side] & bit) position.board[squareOf(index)] = static_cast<uint8_t>(Man | colour);
      if (snapshot.kings[side] & bit) position.board[squareOf(index)] = static_cast<uint8_t>(King | colour);
    }
  }
  position.side = snapshot.side & 1;
  position.jumper = snapshot.jumper < 64 ? snapshot.jumper : kNone;
  position.quiet = snapshot.quiet;
}

void Game::start() {
  pos_.setStart();
  historyCount_ = 0;
  last_ = Step{};
  state_ = State::Playing;
}

bool Game::mustCapture() const {
  if (state_ != State::Playing) return false;
  Step steps[kMaxSteps];
  return generateSteps(pos_, steps) > 0 && steps[0].captured != kNone;
}

int Game::targets(const int from, uint8_t* out) const {
  if (state_ != State::Playing) return 0;
  Step steps[kMaxSteps];
  const int count = generateSteps(pos_, steps);
  int found = 0;
  for (int i = 0; i < count; ++i) {
    if (steps[i].from == from) out[found++] = steps[i].to;
  }
  return found;
}

int Game::movable(uint8_t* out) const {
  if (state_ != State::Playing) return 0;
  Step steps[kMaxSteps];
  const int count = generateSteps(pos_, steps);
  int found = 0;
  for (int i = 0; i < count; ++i) {
    bool listed = false;
    for (int j = 0; j < found; ++j) listed = listed || out[j] == steps[i].from;
    if (!listed) out[found++] = steps[i].from;
  }
  return found;
}

bool Game::play(const int from, const int to) {
  if (state_ != State::Playing) return false;
  Step steps[kMaxSteps];
  const int count = generateSteps(pos_, steps);
  for (int i = 0; i < count; ++i) {
    if (steps[i].from != from || steps[i].to != to) continue;
    if (historyCount_ == kHistory) {
      memmove(history_, history_ + 1, sizeof(Snapshot) * (kHistory - 1));
      --historyCount_;
    }
    history_[historyCount_++] = pack(pos_);
    applyStep(pos_, steps[i]);
    last_ = steps[i];
    updateState();
    return true;
  }
  return false;
}

bool Game::undoTo(const Side side) {
  if (!started()) return false;
  for (int i = historyCount_ - 1; i >= 0; --i) {
    if (history_[i].side != side || history_[i].jumper != kNone) continue;
    unpack(history_[i], pos_);
    historyCount_ = i;
    last_ = Step{};
    state_ = State::Playing;
    return true;
  }
  return false;
}

void Game::updateState() {
  Step steps[kMaxSteps];
  if (generateSteps(pos_, steps) == 0) {
    state_ = pos_.side == Dark ? State::LightWins : State::DarkWins;
    return;
  }
  if (pos_.quiet >= kQuietLimit) {
    state_ = State::Draw;
    return;
  }
  // The same position with the same side to move for the third time.
  if (pos_.jumper == kNone) {
    const Snapshot now = pack(pos_);
    int same = 1;
    for (int i = 0; i < historyCount_; ++i) {
      const Snapshot& old = history_[i];
      if (old.side == now.side && old.jumper == kNone && old.men[0] == now.men[0] && old.men[1] == now.men[1] &&
          old.kings[0] == now.kings[0] && old.kings[1] == now.kings[1]) {
        ++same;
      }
    }
    if (same >= 3) {
      state_ = State::Draw;
      return;
    }
  }
  state_ = State::Playing;
}

size_t Game::save(uint8_t out[kSaveBytes]) const {
  memset(out, 0, kSaveBytes);
  memcpy(out, kSaveMagic, 4);
  out[4] = started() ? 1 : 0;
  // The position now, then as much of the history as fits, oldest first.
  const int kept = historyCount_ < kSavedHistory - 1 ? historyCount_ : kSavedHistory - 1;
  out[5] = static_cast<uint8_t>(kept);
  uint8_t* cursor = out + 8;
  for (int i = -1; i < kept; ++i) {
    const Snapshot snapshot = i < 0 ? pack(pos_) : history_[historyCount_ - kept + i];
    putU32(cursor, snapshot.men[0]);
    putU32(cursor + 4, snapshot.men[1]);
    putU32(cursor + 8, snapshot.kings[0]);
    putU32(cursor + 12, snapshot.kings[1]);
    cursor[16] = snapshot.side;
    cursor[17] = snapshot.jumper;
    cursor[18] = snapshot.quiet;
    cursor += kSnapshotBytes;
  }
  return kSaveBytes;
}

bool Game::load(const uint8_t* data, const size_t size) {
  state_ = State::NotStarted;
  historyCount_ = 0;
  last_ = Step{};
  if (size < kSaveBytes || memcmp(data, kSaveMagic, 4) != 0) return false;
  if (data[4] == 0) return true;
  const int kept = data[5];
  if (kept > kSavedHistory - 1) return false;
  const uint8_t* cursor = data + 8;
  for (int i = -1; i < kept; ++i) {
    Snapshot snapshot;
    snapshot.men[0] = getU32(cursor);
    snapshot.men[1] = getU32(cursor + 4);
    snapshot.kings[0] = getU32(cursor + 8);
    snapshot.kings[1] = getU32(cursor + 12);
    snapshot.side = cursor[16];
    snapshot.jumper = cursor[17];
    snapshot.quiet = cursor[18];
    // No square may hold two pieces.
    if ((snapshot.men[0] & snapshot.men[1]) || (snapshot.kings[0] & snapshot.kings[1]) ||
        ((snapshot.men[0] | snapshot.men[1]) & (snapshot.kings[0] | snapshot.kings[1]))) {
      historyCount_ = 0;
      return false;
    }
    if (i < 0) {
      unpack(snapshot, pos_);
    } else {
      history_[historyCount_++] = snapshot;
    }
    cursor += kSnapshotBytes;
  }
  updateState();
  return true;
}

}  // namespace checkers
