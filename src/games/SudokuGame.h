#pragma once

#include <cstddef>
#include <cstdint>

// Rules, puzzle generator and state of the built-in Sudoku game. No drawing,
// no storage and no heap: the activity owns one Game and persists it through
// save()/load(). Cells are numbered 0..80, row by row.
namespace sudoku {

constexpr int kCells = 81;

// Saved by number: a new level goes at the end.
enum class Difficulty : uint8_t { Easy = 0, Medium = 1, Hard = 2, Expert = 3, Master = 4 };
constexpr int kLevelCount = 5;

struct Puzzle {
  uint8_t givens[kCells] = {};    // 0 = empty
  uint8_t solution[kCells] = {};  // 1..9
};

// Number of solutions of a grid, counted up to limit. firstSolution (optional)
// receives the first one found.
int countSolutions(const uint8_t grid[kCells], int limit, uint8_t* firstSolution = nullptr);

// True when the grid can be completed with naked and hidden singles alone,
// i.e. without guessing or pair/triple reasoning.
bool solvableBySingles(const uint8_t grid[kCells]);

// True when the rules a hint can explain (singles, a number locked in one line
// of a box or one box of a line, naked pairs) complete the grid.
bool solvableByRules(const uint8_t grid[kCells]);

// Makes a puzzle with exactly one solution. Easy and Medium are solvable by
// singles; Hard asks for more than that but no more than the hint rules,
// Expert and Master for more than the hint rules, whenever the seed allows
// it. Master also keeps fewer clues. shouldStop (optional) is asked between
// steps; when it says stop, generate returns false and `out` is not usable.
bool generate(Difficulty difficulty, uint32_t seed, Puzzle& out, bool (*shouldStop)(void*) = nullptr,
              void* context = nullptr);

// Units are numbered 0..8 for the rows, 9..17 for the columns, 18..26 for the boxes.
int unitCell(int unit, int index);

// The next thing a hint can point at, easiest first.
struct Step {
  enum class Kind : uint8_t {
    None,        // no rule fits: the cell's answer can only be shown
    Wrong,       // the digit in the cell is not the answer
    LastInUnit,  // the only empty cell of `unit`
    OnlyNumber,  // every other digit is in the cell's row, column or box
    OnlyPlace,   // the only cell of `unit` that can hold `digit`
  };
  // A harder rule that had to rule candidates out before the single showed.
  enum class Rule : uint8_t { None, LockedLine, Pair };

  Kind kind = Kind::None;
  Rule before = Rule::None;
  int cell = -1;
  uint8_t digit = 0;  // the answer, or the wrong digit for Kind::Wrong
  uint8_t unit = 0;
};

// The step for the grid `values` (solution needed to spot wrong digits).
// preferredCell, when it is open, is explained first if any rule reaches it.
Step nextStep(const uint8_t values[kCells], const uint8_t solution[kCells], int preferredCell);

class Game {
 public:
  enum class Change : uint8_t { None, Placed, Cleared, NoteChanged, Solved };

  // dailyDay: the day number (CalendarMath daysFromDate) of a daily puzzle, 0 for any other.
  void start(const Puzzle& puzzle, Difficulty difficulty, uint16_t dailyDay = 0);
  // The same puzzle from its first clues, with time, mistakes and hints back at zero.
  void restart();

  bool started() const { return started_; }
  bool solved() const { return solved_; }
  Difficulty difficulty() const { return difficulty_; }
  uint16_t dailyDay() const { return dailyDay_; }
  uint8_t value(int cell) const { return values_[cell]; }
  uint8_t solution(int cell) const { return solution_[cell]; }
  const uint8_t* values() const { return values_; }
  const uint8_t* solutionGrid() const { return solution_; }
  bool isGiven(int cell) const { return (flags_[cell] & kGiven) != 0; }
  bool isHinted(int cell) const { return (flags_[cell] & kHinted) != 0; }
  // Given and hinted cells cannot be changed.
  bool isLocked(int cell) const { return flags_[cell] != 0; }
  bool isWrong(int cell) const { return values_[cell] != 0 && values_[cell] != solution_[cell]; }
  // The cell's digit is also in its row, column or box.
  bool repeats(int cell) const;
  // Bit d (1..9) set = pencil note for digit d.
  uint16_t notes(int cell) const { return notes_[cell]; }

  // Writes a digit; the same digit again clears the cell. A wrong digit stays
  // in the cell and counts as a mistake.
  Change place(int cell, uint8_t digit);
  Change toggleNote(int cell, uint8_t digit);
  // Writes into every empty cell the digits its row, column and box do not hold
  // yet, replacing the notes that were there. Returns how many cells changed.
  int fillNotes();
  // Fills each row, column or box of `cell` that has one empty cell left with
  // the digit it is missing, and so on for the units those fill. One undo
  // takes back the whole run together with the move before it. Returns how
  // many cells were filled.
  int fillLastCells(int cell);
  Change erase(int cell);
  bool undo();
  // Fills preferredCell when it is open and not already right, else the first
  // such cell. Returns the cell filled, or -1.
  int hint(int preferredCell);
  // Counts a hint that only explained a step.
  void countHint();

  int emptyCells() const;
  // Cells holding a digit that is not the solution's.
  int wrongCells() const;
  // Erases every wrong digit (each erase can be undone). Returns how many.
  int clearWrong();
  // How many of this digit are still to be placed (correctly or not).
  int remaining(uint8_t digit) const;
  uint16_t mistakes() const { return mistakes_; }
  uint16_t hintsUsed() const { return hints_; }
  uint32_t elapsedSeconds() const { return elapsedSeconds_; }
  void addElapsedSeconds(uint32_t seconds) { elapsedSeconds_ += seconds; }
  // Best solving time per difficulty in seconds, 0 = none yet.
  uint32_t bestSeconds(Difficulty difficulty) const { return bestSeconds_[static_cast<int>(difficulty)]; }
  uint16_t solvedCount(Difficulty difficulty) const { return solvedCount_[static_cast<int>(difficulty)]; }
  // Average solving time per difficulty in seconds, 0 = none yet.
  uint32_t averageSeconds(Difficulty difficulty) const;
  uint16_t totalSolved() const;
  // Daily puzzles: solved on `today`, the run of days in a row that is still
  // alive on `today`, the longest run, and how many were solved in all.
  bool dailySolved(uint16_t today) const { return today != 0 && lastDaily_ == today; }
  uint16_t dailyStreak(uint16_t today) const;
  uint16_t bestDailyStreak() const { return bestStreak_; }
  uint16_t dailyCount() const { return dailyCount_; }

  static constexpr size_t kSaveBytes = 512;
  // The size of the first save format, which load() still reads.
  static constexpr size_t kSaveBytesV1 = 448;
  size_t save(uint8_t out[kSaveBytes]) const;
  bool load(const uint8_t* data, size_t size);

 private:
  static constexpr uint8_t kGiven = 0x01;
  static constexpr uint8_t kHinted = 0x02;
  static constexpr int kUndoDepth = 64;

  struct UndoEntry {
    uint8_t cell;
    uint8_t value;
    uint16_t notes;
    uint32_t peerNotes;  // bit i: peer i lost the note for the digit placed
    uint8_t digit;
    bool linked;  // undone together with the entry before it
  };

  uint8_t values_[kCells] = {};
  uint8_t solution_[kCells] = {};
  uint8_t flags_[kCells] = {};
  uint16_t notes_[kCells] = {};
  UndoEntry undo_[kUndoDepth] = {};
  uint8_t undoCount_ = 0;
  uint8_t undoHead_ = 0;  // next slot to write
  bool started_ = false;
  bool solved_ = false;
  Difficulty difficulty_ = Difficulty::Easy;
  uint16_t mistakes_ = 0;
  uint16_t hints_ = 0;
  uint32_t elapsedSeconds_ = 0;
  uint32_t bestSeconds_[kLevelCount] = {};
  uint16_t solvedCount_[kLevelCount] = {};
  // Time and number of the solves since averages were kept (older saves have none).
  uint32_t totalSeconds_[kLevelCount] = {};
  uint16_t timedCount_[kLevelCount] = {};
  uint16_t dailyDay_ = 0;
  uint16_t lastDaily_ = 0;
  uint16_t streak_ = 0;
  uint16_t bestStreak_ = 0;
  uint16_t dailyCount_ = 0;

  void pushUndo(const UndoEntry& entry);
  // Writes a digit and takes its note out of the peers; returns the undo entry.
  UndoEntry write(int cell, uint8_t digit, bool countMistake);
  Change finishMove(Change change);
};

}  // namespace sudoku
