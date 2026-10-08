#include "SudokuGame.h"

#include <algorithm>
#include <cstring>

namespace sudoku {

namespace {

constexpr uint16_t kAllDigits = 0x3FE;  // bits 1..9
constexpr uint8_t kSaveMagic[4] = {'V', 'S', 'D', '1'};

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

}  // namespace

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
          int cell = 0;
          if (unit < 9) {
            cell = unit * 9 + i;
          } else if (unit < 18) {
            cell = i * 9 + (unit - 9);
          } else {
            const int b = unit - 18;
            cell = ((b / 3) * 3 + i / 3) * 9 + (b % 3) * 3 + i % 3;
          }
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

void generate(const Difficulty difficulty, const uint32_t seed, Puzzle& out) {
  Rng rng(seed);
  // Clues to aim for. Digging stops earlier when no further cell can go
  // without breaking the rule of the level.
  const int targetClues = difficulty == Difficulty::Easy ? 40 : (difficulty == Difficulty::Medium ? 32 : 25);
  const bool singlesOnly = difficulty != Difficulty::Hard;
  // A Hard grid that singles still solve is only kept when no better one turns up.
  const int attempts = difficulty == Difficulty::Hard ? 8 : 1;

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
    for (int i = 0; i < kCells && clues > targetClues; ++i) {
      const int cell = order[i];
      const int mirror = kCells - 1 - cell;
      if (puzzle.givens[cell] == 0) continue;
      const uint8_t keptCell = puzzle.givens[cell];
      const uint8_t keptMirror = puzzle.givens[mirror];
      puzzle.givens[cell] = 0;
      puzzle.givens[mirror] = 0;
      const bool stillGood = singlesOnly ? solvableBySingles(puzzle.givens) : countSolutions(puzzle.givens, 2) == 1;
      if (stillGood) {
        clues -= (mirror == cell || keptMirror == 0) ? 1 : 2;
      } else {
        puzzle.givens[cell] = keptCell;
        puzzle.givens[mirror] = keptMirror;
      }
    }

    out = puzzle;
    if (singlesOnly || !solvableBySingles(puzzle.givens)) return;
  }
}

void Game::start(const Puzzle& puzzle, const Difficulty difficulty) {
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
  return Change::Solved;
}

Game::Change Game::place(const int cell, const uint8_t digit) {
  if (!started_ || solved_ || cell < 0 || cell >= kCells || digit < 1 || digit > 9 || isLocked(cell)) {
    return Change::None;
  }
  if (values_[cell] == digit) return erase(cell);

  UndoEntry entry{static_cast<uint8_t>(cell), values_[cell], notes_[cell], 0, digit};
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
  } else if (mistakes_ < UINT16_MAX) {
    ++mistakes_;
  }
  pushUndo(entry);
  return finishMove(Change::Placed);
}

Game::Change Game::toggleNote(const int cell, const uint8_t digit) {
  if (!started_ || solved_ || cell < 0 || cell >= kCells || digit < 1 || digit > 9 || isLocked(cell) ||
      values_[cell] != 0) {
    return Change::None;
  }
  pushUndo(UndoEntry{static_cast<uint8_t>(cell), 0, notes_[cell], 0, 0});
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
  pushUndo(UndoEntry{static_cast<uint8_t>(cell), values_[cell], notes_[cell], 0, 0});
  values_[cell] = 0;
  notes_[cell] = 0;
  return Change::Cleared;
}

bool Game::undo() {
  if (!started_ || solved_ || undoCount_ == 0) return false;
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
  return true;
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
  for (int level = 0; level < 3; ++level) putU32(cursor, bestSeconds_[level]);
  for (int level = 0; level < 3; ++level) putU16(cursor, solvedCount_[level]);
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
  if (!data || size < kSaveBytes || std::memcmp(data, kSaveMagic, sizeof(kSaveMagic)) != 0) return false;
  const uint8_t* cursor = data + sizeof(kSaveMagic);
  Game loaded;
  loaded.started_ = *cursor++ != 0;
  loaded.solved_ = *cursor++ != 0;
  const uint8_t level = *cursor++;
  ++cursor;
  if (level > static_cast<uint8_t>(Difficulty::Hard)) return false;
  loaded.difficulty_ = static_cast<Difficulty>(level);
  loaded.mistakes_ = getU16(cursor);
  loaded.hints_ = getU16(cursor);
  loaded.elapsedSeconds_ = getU32(cursor);
  for (uint32_t& best : loaded.bestSeconds_) best = getU32(cursor);
  for (uint16_t& count : loaded.solvedCount_) count = getU16(cursor);
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
