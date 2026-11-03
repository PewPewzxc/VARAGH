#include "SudokuGame.h"

#include <algorithm>
#include <cstring>

namespace sudoku {

namespace {

constexpr uint16_t kAllDigits = 0x3FE;  // bits 1..9
constexpr uint8_t kSaveMagic[4] = {'V', 'S', 'D', '2'};
constexpr uint8_t kSaveMagicV1[4] = {'V', 'S', 'D', '1'};

int rowOf(const int cell) { return cell / 9; }
int colOf(const int cell) { return cell % 9; }
int boxOf(const int cell) { return (cell / 27) * 3 + (cell % 9) / 3; }

int popcount(const uint16_t mask) { return __builtin_popcount(mask); }
int lowestDigit(const uint16_t mask) { return __builtin_ctz(mask); }

struct Rng {
  uint32_t state;
  explicit Rng(const uint32_t seed) : state(seed ? seed : 0x9E3779B9u) {}
  uint32_t next() {
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return state;
  }
  uint32_t below(const uint32_t bound) { return next() % bound; }
};

// Digits used in each row, column and box of a grid.
struct Usage {
  uint16_t row[9] = {};
  uint16_t col[9] = {};
  uint16_t box[9] = {};

  // False when the grid repeats a digit in a unit.
  bool load(const uint8_t* grid) {
    for (int cell = 0; cell < kCells; ++cell) {
      if (grid[cell] == 0) continue;
      const uint16_t bit = static_cast<uint16_t>(1u << grid[cell]);
      if ((row[rowOf(cell)] | col[colOf(cell)] | box[boxOf(cell)]) & bit) return false;
      set(cell, bit);
    }
    return true;
  }
  void set(const int cell, const uint16_t bit) {
    row[rowOf(cell)] |= bit;
    col[colOf(cell)] |= bit;
    box[boxOf(cell)] |= bit;
  }
  void clear(const int cell, const uint16_t bit) {
    row[rowOf(cell)] &= static_cast<uint16_t>(~bit);
    col[colOf(cell)] &= static_cast<uint16_t>(~bit);
    box[boxOf(cell)] &= static_cast<uint16_t>(~bit);
  }
  uint16_t candidates(const int cell) const {
    return static_cast<uint16_t>(kAllDigits & ~(row[rowOf(cell)] | col[colOf(cell)] | box[boxOf(cell)]));
  }
};

// Depth-first search on the open cell with the fewest candidates. Iterative:
// the caller may run on a small task stack. With an Rng the digits of a cell
// are tried in random order (used to draw a random full grid).
int search(const uint8_t* input, const int limit, uint8_t* firstSolution, Rng* rng) {
  uint8_t grid[kCells];
  std::memcpy(grid, input, kCells);
  Usage usage;
  if (!usage.load(grid)) return 0;

  struct Frame {
    uint8_t cell;
    uint16_t untried;
  };
  Frame stack[kCells];
  int depth = 0;
  int found = 0;

  auto pickDigit = [&](const uint16_t mask) {
    if (!rng) return lowestDigit(mask);
    int skip = static_cast<int>(rng->below(static_cast<uint32_t>(popcount(mask))));
    uint16_t rest = mask;
    while (skip-- > 0) rest &= static_cast<uint16_t>(rest - 1);
    return lowestDigit(rest);
  };

  bool descend = true;
  while (true) {
    if (descend) {
      int bestCell = -1;
      int bestCount = 10;
      uint16_t bestMask = 0;
      for (int cell = 0; cell < kCells && bestCount > 1; ++cell) {
        if (grid[cell] != 0) continue;
        const uint16_t mask = usage.candidates(cell);
        const int count = popcount(mask);
        if (count < bestCount) {
          bestCount = count;
          bestCell = cell;
          bestMask = mask;
        }
      }
      if (bestCell < 0) {
        if (found == 0 && firstSolution) std::memcpy(firstSolution, grid, kCells);
        if (++found >= limit) return found;
        descend = false;
        continue;
      }
      if (bestCount == 0) {
        descend = false;
        continue;
      }
      stack[depth++] = Frame{static_cast<uint8_t>(bestCell), bestMask};
    }

    // Take the next digit of the top frame, backing out of exhausted ones.
    descend = false;
    while (depth > 0) {
      Frame& frame = stack[depth - 1];
      if (grid[frame.cell] != 0) {
        usage.clear(frame.cell, static_cast<uint16_t>(1u << grid[frame.cell]));
        grid[frame.cell] = 0;
      }
      if (frame.untried == 0) {
        --depth;
        continue;
      }
      const int digit = pickDigit(frame.untried);
      frame.untried &= static_cast<uint16_t>(~(1u << digit));
      grid[frame.cell] = static_cast<uint8_t>(digit);
      usage.set(frame.cell, static_cast<uint16_t>(1u << digit));
      descend = true;
      break;
    }
    if (!descend) return found;
  }
}

// The 20 cells sharing a row, column or box with a cell, in a fixed order.
void peersOf(const int cell, uint8_t out[20]) {
  int count = 0;
  for (int other = 0; other < kCells; ++other) {
    if (other == cell) continue;
    if (rowOf(other) == rowOf(cell) || colOf(other) == colOf(cell) || boxOf(other) == boxOf(cell)) {
      out[count++] = static_cast<uint8_t>(other);
    }
  }
}

void putU16(uint8_t*& out, const uint16_t value) {
  *out++ = static_cast<uint8_t>(value & 0xFF);
  *out++ = static_cast<uint8_t>(value >> 8);
}

void putU32(uint8_t*& out, const uint32_t value) {
  putU16(out, static_cast<uint16_t>(value & 0xFFFF));
  putU16(out, static_cast<uint16_t>(value >> 16));
}

uint16_t getU16(const uint8_t*& in) {
  const uint16_t value = static_cast<uint16_t>(in[0] | (in[1] << 8));
  in += 2;
  return value;
}

uint32_t getU32(const uint8_t*& in) {
  const uint32_t low = getU16(in);
  return low | (static_cast<uint32_t>(getU16(in)) << 16);
}

bool unitHolds(const int unit, const int cell) {
  if (unit < 9) return rowOf(cell) == unit;
  if (unit < 18) return colOf(cell) == unit - 9;
  return boxOf(cell) == unit - 18;
}

// The box, row and column of a cell, in the order a hint names them.
void unitsOf(const int cell, int out[3]) {
  out[0] = 18 + boxOf(cell);
  out[1] = rowOf(cell);
  out[2] = 9 + colOf(cell);
}

// Candidates of every open cell (0 for a filled one), from the digits in its units.
void candidatesOf(const uint8_t* values, uint16_t cand[kCells]) {
  Usage usage;
  for (int cell = 0; cell < kCells; ++cell) {
    if (values[cell] != 0) usage.set(cell, static_cast<uint16_t>(1u << values[cell]));
  }
  for (int cell = 0; cell < kCells; ++cell) cand[cell] = values[cell] != 0 ? 0 : usage.candidates(cell);
}

// A single of one kind in `cell`.
bool singleAt(const uint8_t* values, const uint16_t* cand, const int cell, const Step::Kind kind, Step& out) {
  if (values[cell] != 0 || cand[cell] == 0) return false;
  int units[3];
  unitsOf(cell, units);
  if (kind == Step::Kind::OnlyNumber) {
    if (popcount(cand[cell]) != 1) return false;
    out.kind = kind;
    out.cell = cell;
    out.digit = static_cast<uint8_t>(lowestDigit(cand[cell]));
    out.unit = static_cast<uint8_t>(units[0]);
    return true;
  }
  for (const int unit : units) {
    if (kind == Step::Kind::LastInUnit) {
      int empty = 0;
      for (int i = 0; i < 9; ++i) empty += values[unitCell(unit, i)] == 0 ? 1 : 0;
      if (empty != 1 || popcount(cand[cell]) != 1) continue;
      out.kind = kind;
      out.cell = cell;
      out.digit = static_cast<uint8_t>(lowestDigit(cand[cell]));
      out.unit = static_cast<uint8_t>(unit);
      return true;
    }
    for (int digit = 1; digit <= 9; ++digit) {
      const uint16_t bit = static_cast<uint16_t>(1u << digit);
      if (!(cand[cell] & bit)) continue;
      int places = 0;
      for (int i = 0; i < 9 && places < 2; ++i) places += (cand[unitCell(unit, i)] & bit) ? 1 : 0;
      if (places != 1) continue;
      out.kind = kind;
      out.cell = cell;
      out.digit = static_cast<uint8_t>(digit);
      out.unit = static_cast<uint8_t>(unit);
      return true;
    }
  }
  return false;
}

// The easiest single, in preferredCell when that has one.
bool findSingle(const uint8_t* values, const uint16_t* cand, const int preferredCell, Step& out) {
  constexpr Step::Kind kKinds[] = {Step::Kind::LastInUnit, Step::Kind::OnlyNumber, Step::Kind::OnlyPlace};
  if (preferredCell >= 0) {
    for (const auto kind : kKinds) {
      if (singleAt(values, cand, preferredCell, kind, out)) return true;
    }
  }
  for (const auto kind : kKinds) {
    for (int cell = 0; cell < kCells; ++cell) {
      if (singleAt(values, cand, cell, kind, out)) return true;
    }
  }
  return false;
}

// A digit whose places in a box all lie in one row or column (or whose places
// in a row or column all lie in one box) cannot go anywhere else in that other
// unit. Rules out one such set; true if a candidate went.
bool lockedLine(uint16_t* cand) {
  for (int unit = 0; unit < 27; ++unit) {
    for (int digit = 1; digit <= 9; ++digit) {
      const uint16_t bit = static_cast<uint16_t>(1u << digit);
      int first = -1;
      int places = 0;
      bool sameRow = true;
      bool sameCol = true;
      bool sameBox = true;
      for (int i = 0; i < 9; ++i) {
        const int cell = unitCell(unit, i);
        if (!(cand[cell] & bit)) continue;
        ++places;
        if (first < 0) {
          first = cell;
          continue;
        }
        sameRow = sameRow && rowOf(cell) == rowOf(first);
        sameCol = sameCol && colOf(cell) == colOf(first);
        sameBox = sameBox && boxOf(cell) == boxOf(first);
      }
      if (places < 2) continue;
      int other = -1;
      if (unit >= 18) {
        other = sameRow ? rowOf(first) : (sameCol ? 9 + colOf(first) : -1);
      } else if (sameBox) {
        other = 18 + boxOf(first);
      }
      if (other < 0) continue;
      bool removed = false;
      for (int i = 0; i < 9; ++i) {
        const int cell = unitCell(other, i);
        if (unitHolds(unit, cell) || !(cand[cell] & bit)) continue;
        cand[cell] &= static_cast<uint16_t>(~bit);
        removed = true;
      }
      if (removed) return true;
    }
  }
  return false;
}

// Two cells of a unit that can only hold the same two digits take them from
// the rest of the unit. Rules out one such pair; true if a candidate went.
bool nakedPair(uint16_t* cand) {
  for (int unit = 0; unit < 27; ++unit) {
    for (int i = 0; i < 9; ++i) {
      const int a = unitCell(unit, i);
      if (popcount(cand[a]) != 2) continue;
      for (int j = i + 1; j < 9; ++j) {
        const int b = unitCell(unit, j);
        if (cand[b] != cand[a]) continue;
        bool removed = false;
        for (int k = 0; k < 9; ++k) {
          const int cell = unitCell(unit, k);
          if (cell == a || cell == b || !(cand[cell] & cand[a])) continue;
          cand[cell] &= static_cast<uint16_t>(~cand[a]);
          removed = true;
        }
        if (removed) return true;
      }
    }
  }
  return false;
}

// A single, after the harder rules have ruled candidates out when none shows
// at first. The grid must hold no wrong digit.
bool logicalStep(const uint8_t* values, const int preferredCell, Step& out) {
  uint16_t cand[kCells];
  candidatesOf(values, cand);
  Step::Rule first = Step::Rule::None;
  // Each round rules out at least one of the 729 candidates, so this ends.
  while (true) {
    if (findSingle(values, cand, preferredCell, out)) {
      out.before = first;
      return true;
    }
    Step::Rule used = Step::Rule::None;
    if (lockedLine(cand)) {
      used = Step::Rule::LockedLine;
    } else if (nakedPair(cand)) {
      used = Step::Rule::Pair;
    } else {
      return false;
    }
    if (first == Step::Rule::None) first = used;
  }
}

}  // namespace

int unitCell(const int unit, const int index) {
  if (unit < 9) return unit * 9 + index;
  if (unit < 18) return index * 9 + (unit - 9);
  const int b = unit - 18;
  return ((b / 3) * 3 + index / 3) * 9 + (b % 3) * 3 + index % 3;
}

int countSolutions(const uint8_t grid[kCells], const int limit, uint8_t* firstSolution) {
  return search(grid, limit, firstSolution, nullptr);
}

bool solvableBySingles(const uint8_t input[kCells]) {
  uint8_t grid[kCells];
  std::memcpy(grid, input, kCells);
  Usage usage;
  if (!usage.load(grid)) return false;

  bool progress = true;
  while (progress) {
    progress = false;
    int open = 0;
    for (int cell = 0; cell < kCells; ++cell) {
      if (grid[cell] != 0) continue;
      const uint16_t mask = usage.candidates(cell);
      if (mask == 0) return false;
      if (popcount(mask) == 1) {
        grid[cell] = static_cast<uint8_t>(lowestDigit(mask));
        usage.set(cell, mask);
        progress = true;
      } else {
        ++open;
      }
    }
    if (open == 0) return true;
    if (progress) continue;

    // Hidden singles: a digit with one possible cell in a row, column or box.
    for (int unit = 0; unit < 27 && !progress; ++unit) {
      for (int digit = 1; digit <= 9 && !progress; ++digit) {
        const uint16_t bit = static_cast<uint16_t>(1u << digit);
        int place = -1;
        int places = 0;
        for (int i = 0; i < 9; ++i) {
          const int cell = unitCell(unit, i);
          if (grid[cell] == 0 && (usage.candidates(cell) & bit)) {
            place = cell;
            ++places;
          }
        }
        if (places == 1) {
          grid[place] = static_cast<uint8_t>(digit);
          usage.set(place, bit);
          progress = true;
        }
      }
    }
  }
  return false;
}

bool solvableByRules(const uint8_t input[kCells]) {
  uint8_t grid[kCells];
  std::memcpy(grid, input, kCells);
  Usage usage;
  if (!usage.load(grid)) return false;
  Step step;
  while (std::find(grid, grid + kCells, 0) != grid + kCells) {
    if (!logicalStep(grid, -1, step)) return false;
    grid[step.cell] = step.digit;
  }
  return true;
}

Step nextStep(const uint8_t values[kCells], const uint8_t solution[kCells], const int preferredCell) {
  Step step;
  const bool hasPreferred = preferredCell >= 0 && preferredCell < kCells;
  // A wrong digit first: anything worked out from it would be wrong too.
  int wrong = -1;
  if (hasPreferred && values[preferredCell] != 0 && values[preferredCell] != solution[preferredCell]) {
    wrong = preferredCell;
  }
  for (int cell = 0; cell < kCells && wrong < 0; ++cell) {
    if (values[cell] != 0 && values[cell] != solution[cell]) wrong = cell;
  }
  if (wrong >= 0) {
    step.kind = Step::Kind::Wrong;
    step.cell = wrong;
    step.digit = values[wrong];
    step.unit = static_cast<uint8_t>(18 + boxOf(wrong));
    return step;
  }
  const int preferred = hasPreferred && values[preferredCell] == 0 ? preferredCell : -1;
  if (logicalStep(values, preferred, step)) return step;

  // No rule reaches a cell: the chosen one, else the one with the fewest candidates, can only be shown.
  step = Step{};
  int cell = preferred;
  if (cell < 0) {
    uint16_t cand[kCells];
    candidatesOf(values, cand);
    int fewest = 10;
    for (int i = 0; i < kCells; ++i) {
      if (values[i] == 0 && popcount(cand[i]) < fewest) {
        fewest = popcount(cand[i]);
        cell = i;
      }
    }
  }
  if (cell >= 0) {
    step.cell = cell;
    step.digit = solution[cell];
    step.unit = static_cast<uint8_t>(18 + boxOf(cell));
  }
  return step;
}

bool generate(const Difficulty difficulty, const uint32_t seed, Puzzle& out, bool (*shouldStop)(void*),
              void* context) {
  Rng rng(seed);
  auto stopped = [&] { return shouldStop != nullptr && shouldStop(context); };
  // Clues to aim for. Digging stops earlier when no further cell can go
  // without breaking the rule of the level; Master digs as far as it can.
  constexpr int kTargetClues[kLevelCount] = {40, 32, 25, 24, 17};
  const int level = static_cast<int>(difficulty);
  const int targetClues = kTargetClues[level];
  const bool singlesOnly = difficulty == Difficulty::Easy || difficulty == Difficulty::Medium;
  // How hard a dug grid is: 0 singles solve it, 1 the hint rules do, 2 neither.
  // Hard aims at 1, so a hint can always explain the next step; Expert and
  // Master at 2. A grid off the aim is only kept when no better one turns up
  // (for Hard, 2 before 0).
  const bool beyondRules = difficulty == Difficulty::Expert || difficulty == Difficulty::Master;
  const int aim = singlesOnly ? 0 : (beyondRules ? 2 : 1);
  const int attempts = singlesOnly ? 1 : (beyondRules ? 12 : 8);
  auto rank = [&](const int score) { return score == aim ? 3 : (difficulty == Difficulty::Hard ? score : 0); };
  int bestRank = -1;

  for (int attempt = 0; attempt < attempts; ++attempt) {
    uint8_t empty[kCells] = {};
    Puzzle puzzle;
    search(empty, 1, puzzle.solution, &rng);
    std::memcpy(puzzle.givens, puzzle.solution, kCells);

    // Visit the cells in random order and clear each together with its
    // mirror image through the centre, the classic look of printed puzzles.
    uint8_t order[kCells];
    for (int i = 0; i < kCells; ++i) order[i] = static_cast<uint8_t>(i);
    for (int i = kCells - 1; i > 0; --i) std::swap(order[i], order[rng.below(static_cast<uint32_t>(i + 1))]);

    int clues = kCells;
    for (int i = 0; i < kCells; ++i) {
      if (stopped()) return false;
      // Hard digs on past its clue count while singles alone still solve the grid.
      if (clues <= targetClues && (difficulty != Difficulty::Hard || !solvableBySingles(puzzle.givens))) break;
      const int cell = order[i];
      const int mirror = kCells - 1 - cell;
      if (puzzle.givens[cell] == 0) continue;
      const uint8_t keptCell = puzzle.givens[cell];
      const uint8_t keptMirror = puzzle.givens[mirror];
      puzzle.givens[cell] = 0;
      puzzle.givens[mirror] = 0;
      // Hard digs only while the hint rules still finish the grid (which also
      // means one solution), so a hint can explain every step.
      const bool stillGood = singlesOnly                      ? solvableBySingles(puzzle.givens)
                             : difficulty == Difficulty::Hard ? solvableByRules(puzzle.givens)
                                                              : countSolutions(puzzle.givens, 2) == 1;
      if (stillGood) {
        clues -= (mirror == cell || keptMirror == 0) ? 1 : 2;
      } else {
        puzzle.givens[cell] = keptCell;
        puzzle.givens[mirror] = keptMirror;
      }
    }

    // Master then gives up the mirror look and digs out single cells too, and
    // so does Hard while singles alone still solve its grid.
    for (int i = 0; difficulty == Difficulty::Master && i < kCells; ++i) {
      if (stopped()) return false;
      const int cell = order[i];
      const uint8_t kept = puzzle.givens[cell];
      if (kept == 0) continue;
      puzzle.givens[cell] = 0;
      if (countSolutions(puzzle.givens, 2) != 1) puzzle.givens[cell] = kept;
    }
    for (int i = 0; difficulty == Difficulty::Hard && i < kCells && solvableBySingles(puzzle.givens); ++i) {
      if (stopped()) return false;
      const int cell = order[i];
      const uint8_t kept = puzzle.givens[cell];
      if (kept == 0) continue;
      puzzle.givens[cell] = 0;
      if (!solvableByRules(puzzle.givens)) puzzle.givens[cell] = kept;
    }

    int score = 0;
    if (!singlesOnly && !solvableBySingles(puzzle.givens)) score = solvableByRules(puzzle.givens) ? 1 : 2;
    if (rank(score) > bestRank) {
      out = puzzle;
      bestRank = rank(score);
    }
    if (bestRank == 3) return true;
  }
  return true;
}

void Game::start(const Puzzle& puzzle, const Difficulty difficulty, const uint16_t dailyDay) {
  dailyDay_ = dailyDay;
  std::memcpy(values_, puzzle.givens, kCells);
  std::memcpy(solution_, puzzle.solution, kCells);
  for (int cell = 0; cell < kCells; ++cell) {
    flags_[cell] = puzzle.givens[cell] != 0 ? kGiven : 0;
    notes_[cell] = 0;
  }
  undoCount_ = 0;
  undoHead_ = 0;
  started_ = true;
  solved_ = false;
  difficulty_ = difficulty;
  mistakes_ = 0;
  hints_ = 0;
  elapsedSeconds_ = 0;
}

void Game::restart() {
  if (!started_ || solved_) return;
  for (int cell = 0; cell < kCells; ++cell) {
    if (!isGiven(cell)) values_[cell] = 0;
    flags_[cell] &= kGiven;
    notes_[cell] = 0;
  }
  undoCount_ = 0;
  undoHead_ = 0;
  mistakes_ = 0;
  hints_ = 0;
  elapsedSeconds_ = 0;
}

void Game::pushUndo(const UndoEntry& entry) {
  undo_[undoHead_] = entry;
  undoHead_ = static_cast<uint8_t>((undoHead_ + 1) % kUndoDepth);
  if (undoCount_ < kUndoDepth) ++undoCount_;
}

Game::Change Game::finishMove(const Change change) {
  if (change != Change::Placed || solved_) return change;
  for (int cell = 0; cell < kCells; ++cell) {
    if (values_[cell] != solution_[cell]) return change;
  }
  solved_ = true;
  // The undo history would let a finished grid be reopened.
  undoCount_ = 0;
  const int level = static_cast<int>(difficulty_);
  if (solvedCount_[level] < UINT16_MAX) ++solvedCount_[level];
  if (bestSeconds_[level] == 0 || elapsedSeconds_ < bestSeconds_[level]) {
    bestSeconds_[level] = std::max<uint32_t>(1, elapsedSeconds_);
  }
  if (timedCount_[level] < UINT16_MAX && totalSeconds_[level] <= UINT32_MAX - elapsedSeconds_) {
    ++timedCount_[level];
    totalSeconds_[level] += elapsedSeconds_;
  }
  // A daily puzzle the day after the last one solved carries the run on.
  if (dailyDay_ != 0 && dailyDay_ > lastDaily_) {
    streak_ = lastDaily_ != 0 && lastDaily_ + 1 == dailyDay_ ? static_cast<uint16_t>(streak_ + 1) : 1;
    bestStreak_ = std::max(bestStreak_, streak_);
    lastDaily_ = dailyDay_;
    if (dailyCount_ < UINT16_MAX) ++dailyCount_;
  }
  return Change::Solved;
}

uint32_t Game::averageSeconds(const Difficulty difficulty) const {
  const int level = static_cast<int>(difficulty);
  return timedCount_[level] == 0 ? 0 : totalSeconds_[level] / timedCount_[level];
}

uint16_t Game::totalSolved() const {
  uint32_t total = 0;
  for (const uint16_t count : solvedCount_) total += count;
  return static_cast<uint16_t>(std::min<uint32_t>(total, UINT16_MAX));
}

uint16_t Game::dailyStreak(const uint16_t today) const {
  return lastDaily_ != 0 && (lastDaily_ == today || lastDaily_ + 1 == today) ? streak_ : 0;
}

bool Game::repeats(const int cell) const {
  if (cell < 0 || cell >= kCells || values_[cell] == 0) return false;
  uint8_t peers[20];
  peersOf(cell, peers);
  for (const uint8_t peer : peers) {
    if (values_[peer] == values_[cell]) return true;
  }
  return false;
}

Game::UndoEntry Game::write(const int cell, const uint8_t digit, const bool countMistake) {
  UndoEntry entry{static_cast<uint8_t>(cell), values_[cell], notes_[cell], 0, digit, false};
  values_[cell] = digit;
  notes_[cell] = 0;
  if (digit == solution_[cell]) {
    // A digit that is right rules itself out for the cells it shares a unit with.
    uint8_t peers[20];
    peersOf(cell, peers);
    const uint16_t bit = static_cast<uint16_t>(1u << digit);
    for (int i = 0; i < 20; ++i) {
      if (notes_[peers[i]] & bit) {
        notes_[peers[i]] &= static_cast<uint16_t>(~bit);
        entry.peerNotes |= 1u << i;
      }
    }
  } else if (countMistake && mistakes_ < UINT16_MAX) {
    ++mistakes_;
  }
  return entry;
}

Game::Change Game::place(const int cell, const uint8_t digit) {
  if (!started_ || solved_ || cell < 0 || cell >= kCells || digit < 1 || digit > 9 || isLocked(cell)) {
    return Change::None;
  }
  if (values_[cell] == digit) return erase(cell);
  pushUndo(write(cell, digit, true));
  return finishMove(Change::Placed);
}

int Game::fillLastCells(const int cell) {
  if (!started_ || solved_ || cell < 0 || cell >= kCells || values_[cell] == 0) return 0;
  // Each cell filled may leave one of its own units one cell short.
  uint8_t queue[kCells + 1];
  int head = 0;
  int tail = 0;
  queue[tail++] = static_cast<uint8_t>(cell);
  int filled = 0;
  while (head < tail) {
    int units[3];
    unitsOf(queue[head++], units);
    for (const int unit : units) {
      int empty = -1;
      int open = 0;
      uint16_t used = 0;
      for (int i = 0; i < 9; ++i) {
        const int other = unitCell(unit, i);
        if (values_[other] == 0) {
          empty = other;
          ++open;
        } else {
          used |= static_cast<uint16_t>(1u << values_[other]);
        }
      }
      const uint16_t missing = static_cast<uint16_t>(kAllDigits & ~used);
      // A repeated digit leaves two missing: then there is nothing sure to write.
      if (open != 1 || popcount(missing) != 1) continue;
      UndoEntry entry = write(empty, static_cast<uint8_t>(lowestDigit(missing)), false);
      entry.linked = true;
      pushUndo(entry);
      queue[tail++] = static_cast<uint8_t>(empty);
      ++filled;
    }
  }
  if (filled > 0) finishMove(Change::Placed);
  return filled;
}

Game::Change Game::toggleNote(const int cell, const uint8_t digit) {
  if (!started_ || solved_ || cell < 0 || cell >= kCells || digit < 1 || digit > 9 || isLocked(cell) ||
      values_[cell] != 0) {
    return Change::None;
  }
  pushUndo(UndoEntry{static_cast<uint8_t>(cell), 0, notes_[cell], 0, 0, false});
  notes_[cell] ^= static_cast<uint16_t>(1u << digit);
  return Change::NoteChanged;
}

int Game::fillNotes() {
  if (!started_ || solved_) return 0;
  int changed = 0;
  for (int cell = 0; cell < kCells; ++cell) {
    if (values_[cell] != 0) continue;
    uint8_t peers[20];
    peersOf(cell, peers);
    uint16_t possible = kAllDigits;
    for (int i = 0; i < 20; ++i) possible &= static_cast<uint16_t>(~(1u << values_[peers[i]]));
    if (notes_[cell] != possible) {
      notes_[cell] = possible;
      ++changed;
    }
  }
  return changed;
}

Game::Change Game::erase(const int cell) {
  if (!started_ || solved_ || cell < 0 || cell >= kCells || isLocked(cell)) return Change::None;
  if (values_[cell] == 0 && notes_[cell] == 0) return Change::None;
  pushUndo(UndoEntry{static_cast<uint8_t>(cell), values_[cell], notes_[cell], 0, 0, false});
  values_[cell] = 0;
  notes_[cell] = 0;
  return Change::Cleared;
}

bool Game::undo() {
  if (!started_ || solved_ || undoCount_ == 0) return false;
  bool linked = true;
  while (linked && undoCount_ > 0) {
    undoHead_ = static_cast<uint8_t>((undoHead_ + kUndoDepth - 1) % kUndoDepth);
    --undoCount_;
    const UndoEntry& entry = undo_[undoHead_];
    values_[entry.cell] = entry.value;
    notes_[entry.cell] = entry.notes;
    if (entry.peerNotes != 0) {
      uint8_t peers[20];
      peersOf(entry.cell, peers);
      for (int i = 0; i < 20; ++i) {
        if (entry.peerNotes & (1u << i)) notes_[peers[i]] |= static_cast<uint16_t>(1u << entry.digit);
      }
    }
    linked = entry.linked;
  }
  return true;
}

void Game::countHint() {
  if (hints_ < UINT16_MAX) ++hints_;
}

int Game::hint(const int preferredCell) {
  if (!started_ || solved_) return -1;
  auto open = [this](const int cell) { return !isLocked(cell) && values_[cell] != solution_[cell]; };
  int cell = -1;
  if (preferredCell >= 0 && preferredCell < kCells && open(preferredCell)) {
    cell = preferredCell;
  } else {
    for (int i = 0; i < kCells && cell < 0; ++i) {
      if (open(i)) cell = i;
    }
  }
  if (cell < 0) return -1;

  const uint8_t digit = solution_[cell];
  values_[cell] = digit;
  notes_[cell] = 0;
  flags_[cell] |= kHinted;
  uint8_t peers[20];
  peersOf(cell, peers);
  for (int i = 0; i < 20; ++i) notes_[peers[i]] &= static_cast<uint16_t>(~(1u << digit));
  if (hints_ < UINT16_MAX) ++hints_;
  // A hint cannot be taken back, so earlier moves must not rewrite its cell.
  undoCount_ = 0;
  finishMove(Change::Placed);
  return cell;
}

int Game::emptyCells() const {
  int count = 0;
  for (int cell = 0; cell < kCells; ++cell) count += values_[cell] == 0 ? 1 : 0;
  return count;
}

int Game::wrongCells() const {
  int count = 0;
  for (int cell = 0; cell < kCells; ++cell) count += isWrong(cell) ? 1 : 0;
  return count;
}

int Game::clearWrong() {
  int count = 0;
  for (int cell = 0; cell < kCells; ++cell) {
    if (isWrong(cell) && erase(cell) == Change::Cleared) ++count;
  }
  return count;
}

int Game::remaining(const uint8_t digit) const {
  int placed = 0;
  for (int cell = 0; cell < kCells; ++cell) placed += values_[cell] == digit ? 1 : 0;
  return std::max(0, 9 - placed);
}

size_t Game::save(uint8_t out[kSaveBytes]) const {
  std::memset(out, 0, kSaveBytes);
  uint8_t* cursor = out;
  std::memcpy(cursor, kSaveMagic, sizeof(kSaveMagic));
  cursor += sizeof(kSaveMagic);
  *cursor++ = started_ ? 1 : 0;
  *cursor++ = solved_ ? 1 : 0;
  *cursor++ = static_cast<uint8_t>(difficulty_);
  *cursor++ = 0;
  putU16(cursor, mistakes_);
  putU16(cursor, hints_);
  putU32(cursor, elapsedSeconds_);
  for (const uint32_t best : bestSeconds_) putU32(cursor, best);
  for (const uint16_t count : solvedCount_) putU16(cursor, count);
  for (const uint32_t total : totalSeconds_) putU32(cursor, total);
  for (const uint16_t count : timedCount_) putU16(cursor, count);
  for (const uint16_t field : {dailyDay_, lastDaily_, streak_, bestStreak_, dailyCount_}) putU16(cursor, field);
  std::memcpy(cursor, values_, kCells);
  cursor += kCells;
  std::memcpy(cursor, solution_, kCells);
  cursor += kCells;
  std::memcpy(cursor, flags_, kCells);
  cursor += kCells;
  for (int cell = 0; cell < kCells; ++cell) putU16(cursor, notes_[cell]);
  return kSaveBytes;
}

bool Game::load(const uint8_t* data, const size_t size) {
  if (!data || size < sizeof(kSaveMagic)) return false;
  const bool current = std::memcmp(data, kSaveMagic, sizeof(kSaveMagic)) == 0;
  // The first format: three levels, no averages, no daily puzzles.
  const bool first = !current && std::memcmp(data, kSaveMagicV1, sizeof(kSaveMagicV1)) == 0;
  if (!(current ? size >= kSaveBytes : first && size >= kSaveBytesV1)) return false;
  const int levels = current ? kLevelCount : 3;
  const uint8_t* cursor = data + sizeof(kSaveMagic);
  Game loaded;
  loaded.started_ = *cursor++ != 0;
  loaded.solved_ = *cursor++ != 0;
  const uint8_t level = *cursor++;
  ++cursor;
  if (level >= levels) return false;
  loaded.difficulty_ = static_cast<Difficulty>(level);
  loaded.mistakes_ = getU16(cursor);
  loaded.hints_ = getU16(cursor);
  loaded.elapsedSeconds_ = getU32(cursor);
  for (int i = 0; i < levels; ++i) loaded.bestSeconds_[i] = getU32(cursor);
  for (int i = 0; i < levels; ++i) loaded.solvedCount_[i] = getU16(cursor);
  if (current) {
    for (uint32_t& total : loaded.totalSeconds_) total = getU32(cursor);
    for (uint16_t& count : loaded.timedCount_) count = getU16(cursor);
    for (uint16_t* field : {&loaded.dailyDay_, &loaded.lastDaily_, &loaded.streak_, &loaded.bestStreak_,
                            &loaded.dailyCount_}) {
      *field = getU16(cursor);
    }
  }
  std::memcpy(loaded.values_, cursor, kCells);
  cursor += kCells;
  std::memcpy(loaded.solution_, cursor, kCells);
  cursor += kCells;
  std::memcpy(loaded.flags_, cursor, kCells);
  cursor += kCells;
  for (uint16_t& notes : loaded.notes_) notes = static_cast<uint16_t>(getU16(cursor) & kAllDigits);

  if (loaded.started_) {
    for (int cell = 0; cell < kCells; ++cell) {
      const bool locked = (loaded.flags_[cell] & (kGiven | kHinted)) != 0;
      if (loaded.values_[cell] > 9 || loaded.solution_[cell] < 1 || loaded.solution_[cell] > 9 ||
          (loaded.flags_[cell] & ~(kGiven | kHinted)) != 0 ||
          (locked && loaded.values_[cell] != loaded.solution_[cell])) {
        return false;
      }
    }
    if (countSolutions(loaded.solution_, 1) != 1) return false;
  }
  *this = loaded;
  return true;
}

}  // namespace sudoku
