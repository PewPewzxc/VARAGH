#include <gtest/gtest.h>

#include <cstring>

#include "src/games/SudokuGame.h"

namespace {

int clueCount(const sudoku::Puzzle& puzzle) {
  int count = 0;
  for (const uint8_t given : puzzle.givens) count += given != 0 ? 1 : 0;
  return count;
}

void expectValidSolution(const uint8_t* grid) {
  for (int unit = 0; unit < 9; ++unit) {
    int row = 0;
    int col = 0;
    int box = 0;
    for (int i = 0; i < 9; ++i) {
      row |= 1 << grid[unit * 9 + i];
      col |= 1 << grid[i * 9 + unit];
      box |= 1 << grid[((unit / 3) * 3 + i / 3) * 9 + (unit % 3) * 3 + i % 3];
    }
    EXPECT_EQ(row, 0x3FE);
    EXPECT_EQ(col, 0x3FE);
    EXPECT_EQ(box, 0x3FE);
  }
}

int firstOpenCell(const sudoku::Game& game) {
  for (int cell = 0; cell < sudoku::kCells; ++cell) {
    if (!game.isLocked(cell) && game.value(cell) == 0) return cell;
  }
  return -1;
}

}  // namespace

TEST(SudokuSolver, CountsSolutions) {
  const char* text =
      "53..7...."
      "6..195..."
      ".98....6."
      "8...6...3"
      "4..8.3..1"
      "7...2...6"
      ".6....28."
      "...419..5"
      "....8..79";
  uint8_t grid[sudoku::kCells];
  for (int i = 0; i < sudoku::kCells; ++i) grid[i] = text[i] == '.' ? 0 : static_cast<uint8_t>(text[i] - '0');
  uint8_t solution[sudoku::kCells];
  EXPECT_EQ(sudoku::countSolutions(grid, 2, solution), 1);
  expectValidSolution(solution);
  EXPECT_EQ(solution[2], 4);
  EXPECT_TRUE(sudoku::solvableBySingles(grid));

  uint8_t empty[sudoku::kCells] = {};
  EXPECT_EQ(sudoku::countSolutions(empty, 2), 2);
  EXPECT_FALSE(sudoku::solvableBySingles(empty));

  grid[1] = 5;  // two fives in the first row
  EXPECT_EQ(sudoku::countSolutions(grid, 2), 0);
}

TEST(SudokuGenerator, EveryPuzzleHasExactlyOneSolution) {
  const sudoku::Difficulty levels[] = {sudoku::Difficulty::Easy, sudoku::Difficulty::Medium, sudoku::Difficulty::Hard};
  int hardBeyondSingles = 0;
  for (uint32_t seed = 1; seed <= 40; ++seed) {
    for (const sudoku::Difficulty level : levels) {
      sudoku::Puzzle puzzle;
      sudoku::generate(level, seed * 2654435761u, puzzle);
      expectValidSolution(puzzle.solution);
      uint8_t solved[sudoku::kCells];
      ASSERT_EQ(sudoku::countSolutions(puzzle.givens, 2, solved), 1) << "seed " << seed;
      EXPECT_EQ(std::memcmp(solved, puzzle.solution, sudoku::kCells), 0);
      for (int cell = 0; cell < sudoku::kCells; ++cell) {
        if (puzzle.givens[cell] != 0) {
          EXPECT_EQ(puzzle.givens[cell], puzzle.solution[cell]);
        }
      }
      const int clues = clueCount(puzzle);
      if (level == sudoku::Difficulty::Hard) {
        EXPECT_LE(clues, 32) << "seed " << seed;
        hardBeyondSingles += sudoku::solvableBySingles(puzzle.givens) ? 0 : 1;
      } else {
        EXPECT_TRUE(sudoku::solvableBySingles(puzzle.givens)) << "seed " << seed;
        EXPECT_GE(clues, level == sudoku::Difficulty::Easy ? 39 : 24) << "seed " << seed;
        EXPECT_LE(clues, level == sudoku::Difficulty::Easy ? 41 : 40) << "seed " << seed;
      }
    }
  }
  EXPECT_GE(hardBeyondSingles, 36);
}

TEST(SudokuGenerator, SameSeedGivesTheSamePuzzle) {
  sudoku::Puzzle a;
  sudoku::Puzzle b;
  sudoku::Puzzle c;
  sudoku::generate(sudoku::Difficulty::Medium, 1234, a);
  sudoku::generate(sudoku::Difficulty::Medium, 1234, b);
  sudoku::generate(sudoku::Difficulty::Medium, 1235, c);
  EXPECT_EQ(std::memcmp(a.givens, b.givens, sudoku::kCells), 0);
  EXPECT_NE(std::memcmp(a.solution, c.solution, sudoku::kCells), 0);
}

TEST(SudokuGame, PlacesErasesAndUndoes) {
  sudoku::Puzzle puzzle;
  sudoku::generate(sudoku::Difficulty::Easy, 99, puzzle);
  sudoku::Game game;
  game.start(puzzle, sudoku::Difficulty::Easy);
  ASSERT_TRUE(game.started());

  int given = 0;
  while (!game.isGiven(given)) ++given;
  EXPECT_EQ(game.place(given, 1), sudoku::Game::Change::None);
  EXPECT_EQ(game.erase(given), sudoku::Game::Change::None);

  const int cell = firstOpenCell(game);
  ASSERT_GE(cell, 0);
  const uint8_t right = game.solution(cell);
  const uint8_t wrong = static_cast<uint8_t>(right % 9 + 1);
  const int emptyBefore = game.emptyCells();

  EXPECT_EQ(game.place(cell, wrong), sudoku::Game::Change::Placed);
  EXPECT_TRUE(game.isWrong(cell));
  EXPECT_EQ(game.mistakes(), 1);
  EXPECT_EQ(game.emptyCells(), emptyBefore - 1);
  EXPECT_EQ(game.place(cell, wrong), sudoku::Game::Change::Cleared);  // same digit again clears
  EXPECT_EQ(game.value(cell), 0);

  EXPECT_EQ(game.place(cell, right), sudoku::Game::Change::Placed);
  EXPECT_FALSE(game.isWrong(cell));
  EXPECT_EQ(game.mistakes(), 1);
  EXPECT_TRUE(game.undo());  // back to empty
  EXPECT_EQ(game.value(cell), 0);
  EXPECT_TRUE(game.undo());  // back to the wrong digit
  EXPECT_EQ(game.value(cell), wrong);
  EXPECT_TRUE(game.undo());
  EXPECT_EQ(game.value(cell), 0);
  EXPECT_FALSE(game.undo());
}

TEST(SudokuGame, NotesFollowPlacedDigitsAndUndo) {
  sudoku::Puzzle puzzle;
  sudoku::generate(sudoku::Difficulty::Medium, 7, puzzle);
  sudoku::Game game;
  game.start(puzzle, sudoku::Difficulty::Medium);

  // Two open cells in one row.
  int first = -1;
  int second = -1;
  for (int row = 0; row < 9 && second < 0; ++row) {
    first = -1;
    for (int col = 0; col < 9; ++col) {
      const int cell = row * 9 + col;
      if (game.isLocked(cell)) continue;
      if (first < 0) {
        first = cell;
      } else {
        second = cell;
        break;
      }
    }
  }
  ASSERT_GE(second, 0);
  const uint8_t digit = game.solution(first);

  EXPECT_EQ(game.toggleNote(second, digit), sudoku::Game::Change::NoteChanged);
  EXPECT_EQ(game.toggleNote(second, 9), sudoku::Game::Change::NoteChanged);
  EXPECT_TRUE(game.notes(second) & (1u << digit));

  EXPECT_EQ(game.place(first, digit), sudoku::Game::Change::Placed);
  EXPECT_FALSE(game.notes(second) & (1u << digit));  // ruled out by the digit beside it
  if (digit != 9) {
    EXPECT_TRUE(game.notes(second) & (1u << 9));
  }

  EXPECT_TRUE(game.undo());
  EXPECT_TRUE(game.notes(second) & (1u << digit));
  EXPECT_EQ(game.toggleNote(first, 3), sudoku::Game::Change::NoteChanged);
  EXPECT_EQ(game.place(first, digit), sudoku::Game::Change::Placed);
  EXPECT_EQ(game.notes(first), 0);
  EXPECT_EQ(game.toggleNote(first, 3), sudoku::Game::Change::None);  // no notes under a digit
}

TEST(SudokuGame, FillNotesListsWhatEachOpenCellCanStillHold) {
  sudoku::Puzzle puzzle;
  sudoku::generate(sudoku::Difficulty::Medium, 11, puzzle);
  sudoku::Game game;
  game.start(puzzle, sudoku::Difficulty::Medium);

  EXPECT_EQ(game.fillNotes(), game.emptyCells());
  EXPECT_EQ(game.fillNotes(), 0);
  for (int cell = 0; cell < sudoku::kCells; ++cell) {
    if (game.value(cell) != 0) {
      EXPECT_EQ(game.notes(cell), 0);
      continue;
    }
    // The right digit is always among the notes, and no note repeats a digit of the row.
    EXPECT_TRUE(game.notes(cell) & (1u << game.solution(cell)));
    for (int col = 0; col < 9; ++col) {
      const uint8_t inRow = game.value((cell / 9) * 9 + col);
      if (inRow != 0) {
        EXPECT_FALSE(game.notes(cell) & (1u << inRow));
      }
    }
  }

  // An answer takes its digit out of the notes around it, as with hand-written notes.
  const int cell = firstOpenCell(game);
  const uint8_t digit = game.solution(cell);
  game.place(cell, digit);
  for (int col = 0; col < 9; ++col) {
    EXPECT_FALSE(game.notes((cell / 9) * 9 + col) & (1u << digit));
  }
}

TEST(SudokuGame, SolvingRecordsTheBestTime) {
  sudoku::Puzzle puzzle;
  sudoku::generate(sudoku::Difficulty::Hard, 5, puzzle);
  sudoku::Game game;
  game.start(puzzle, sudoku::Difficulty::Hard);
  game.addElapsedSeconds(321);

  EXPECT_EQ(game.remaining(1) + game.remaining(2) + game.remaining(3) + game.remaining(4) + game.remaining(5) +
                game.remaining(6) + game.remaining(7) + game.remaining(8) + game.remaining(9),
            game.emptyCells());

  const int hinted = game.hint(-1);
  ASSERT_GE(hinted, 0);
  EXPECT_TRUE(game.isHinted(hinted));
  EXPECT_TRUE(game.isLocked(hinted));
  EXPECT_EQ(game.hintsUsed(), 1);
  EXPECT_FALSE(game.undo());

  sudoku::Game::Change last = sudoku::Game::Change::None;
  for (int cell = 0; cell < sudoku::kCells; ++cell) {
    if (game.value(cell) == 0) last = game.place(cell, game.solution(cell));
  }
  EXPECT_EQ(last, sudoku::Game::Change::Solved);
  EXPECT_TRUE(game.solved());
  EXPECT_EQ(game.bestSeconds(sudoku::Difficulty::Hard), 321u);
  EXPECT_EQ(game.solvedCount(sudoku::Difficulty::Hard), 1);
  EXPECT_EQ(game.place(0, 1), sudoku::Game::Change::None);
  EXPECT_EQ(game.hint(-1), -1);

  // A slower later round keeps the record; the count goes up.
  sudoku::generate(sudoku::Difficulty::Hard, 6, puzzle);
  game.start(puzzle, sudoku::Difficulty::Hard);
  game.addElapsedSeconds(900);
  for (int cell = 0; cell < sudoku::kCells; ++cell) {
    if (game.value(cell) == 0) game.place(cell, game.solution(cell));
  }
  EXPECT_TRUE(game.solved());
  EXPECT_EQ(game.bestSeconds(sudoku::Difficulty::Hard), 321u);
  EXPECT_EQ(game.solvedCount(sudoku::Difficulty::Hard), 2);
}

TEST(SudokuGame, SaveAndLoadRestoreTheGrid) {
  sudoku::Puzzle puzzle;
  sudoku::generate(sudoku::Difficulty::Easy, 42, puzzle);
  sudoku::Game game;
  game.start(puzzle, sudoku::Difficulty::Easy);
  const int cell = firstOpenCell(game);
  game.place(cell, game.solution(cell));
  const int noteCell = firstOpenCell(game);
  game.toggleNote(noteCell, 4);
  game.toggleNote(noteCell, 7);
  game.addElapsedSeconds(75);

  uint8_t bytes[sudoku::Game::kSaveBytes];
  ASSERT_EQ(game.save(bytes), sizeof(bytes));
  sudoku::Game restored;
  ASSERT_TRUE(restored.load(bytes, sizeof(bytes)));
  EXPECT_TRUE(restored.started());
  EXPECT_EQ(restored.difficulty(), sudoku::Difficulty::Easy);
  EXPECT_EQ(restored.elapsedSeconds(), 75u);
  EXPECT_EQ(restored.notes(noteCell), (1u << 4) | (1u << 7));
  for (int i = 0; i < sudoku::kCells; ++i) {
    EXPECT_EQ(restored.value(i), game.value(i));
    EXPECT_EQ(restored.isGiven(i), game.isGiven(i));
  }
  EXPECT_FALSE(restored.undo());  // history is not saved

  bytes[10 + 24] = 12;  // a cell value out of range
  sudoku::Game untouched;
  EXPECT_FALSE(untouched.load(bytes, sizeof(bytes)));
  EXPECT_FALSE(untouched.started());
  EXPECT_FALSE(untouched.load(bytes, 100));
}
