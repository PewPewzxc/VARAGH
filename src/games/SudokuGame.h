#pragma once

#include <cstddef>
#include <cstdint>

// Rules, puzzle generator and state of the built-in Sudoku game. No drawing,
// no storage and no heap: the activity owns one Game and persists it through
// save()/load(). Cells are numbered 0..80, row by row.
namespace sudoku {

constexpr int kCells = 81;

enum class Difficulty : uint8_t { Easy = 0, Medium = 1, Hard = 2 };

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

// Makes a puzzle with exactly one solution. Easy and Medium are solvable by
// singles; Hard asks for more than that whenever the seed allows it.
void generate(Difficulty difficulty, uint32_t seed, Puzzle& out);

class Game {
 public:
  enum class Change : uint8_t { None, Placed, Cleared, NoteChanged, Solved };

  void start(const Puzzle& puzzle, Difficulty difficulty);

  bool started() const { return started_; }
  bool solved() const { return solved_; }
  Difficulty difficulty() const { return difficulty_; }
  uint8_t value(int cell) const { return values_[cell]; }
  uint8_t solution(int cell) const { return solution_[cell]; }
  bool isGiven(int cell) const { return (flags_[cell] & kGiven) != 0; }
  bool isHinted(int cell) const { return (flags_[cell] & kHinted) != 0; }
  // Given and hinted cells cannot be changed.
  bool isLocked(int cell) const { return flags_[cell] != 0; }
  bool isWrong(int cell) const { return values_[cell] != 0 && values_[cell] != solution_[cell]; }
  // Bit d (1..9) set = pencil note for digit d.
  uint16_t notes(int cell) const { return notes_[cell]; }

  // Writes a digit; the same digit again clears the cell. A wrong digit stays
  // in the cell and counts as a mistake.
  Change place(int cell, uint8_t digit);
  Change toggleNote(int cell, uint8_t digit);
  // Writes into every empty cell the digits its row, column and box do not hold
  // yet, replacing the notes that were there. Returns how many cells changed.
  int fillNotes();
  Change erase(int cell);
  bool undo();
  // Fills preferredCell when it is open and not already right, else the first
  // such cell. Returns the cell filled, or -1.
  int hint(int preferredCell);

  int emptyCells() const;
  // How many of this digit are still to be placed (correctly or not).
  int remaining(uint8_t digit) const;
  uint16_t mistakes() const { return mistakes_; }
  uint16_t hintsUsed() const { return hints_; }
  uint32_t elapsedSeconds() const { return elapsedSeconds_; }
  void addElapsedSeconds(uint32_t seconds) { elapsedSeconds_ += seconds; }
  // Best solving time per difficulty in seconds, 0 = none yet.
  uint32_t bestSeconds(Difficulty difficulty) const { return bestSeconds_[static_cast<int>(difficulty)]; }
  uint16_t solvedCount(Difficulty difficulty) const { return solvedCount_[static_cast<int>(difficulty)]; }

  static constexpr size_t kSaveBytes = 448;
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
  uint32_t bestSeconds_[3] = {};
  uint16_t solvedCount_[3] = {};

  void pushUndo(const UndoEntry& entry);
  Change finishMove(Change change);
};

}  // namespace sudoku
