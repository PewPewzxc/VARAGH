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

// Where the grid starts in the save file (after magic, state, records and daily runs).
constexpr int kValuesOffset = 86;

void solveRest(sudoku::Game& game) {
  for (int cell = 0; cell < sudoku::kCells; ++cell) {
    if (game.value(cell) != game.solution(cell)) game.place(cell, game.solution(cell));
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
  int hardByRules = 0;
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
        hardByRules += sudoku::solvableByRules(puzzle.givens) ? 1 : 0;
      } else {
        EXPECT_TRUE(sudoku::solvableBySingles(puzzle.givens)) << "seed " << seed;
        EXPECT_GE(clues, level == sudoku::Difficulty::Easy ? 39 : 24) << "seed " << seed;
        EXPECT_LE(clues, level == sudoku::Difficulty::Easy ? 41 : 40) << "seed " << seed;
      }
    }
  }
  EXPECT_GE(hardBeyondSingles, 36);
  EXPECT_GE(hardByRules, 39);  // a hint can explain every step of a Hard puzzle
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

TEST(SudokuGame, FullGridWithMistakesCountsAndClearsThem) {
  sudoku::Puzzle puzzle;
  sudoku::generate(sudoku::Difficulty::Easy, 42, puzzle);
  sudoku::Game game;
  game.start(puzzle, sudoku::Difficulty::Easy);

  // Fill every open cell, two of them wrong.
  int wrongPlaced = 0;
  int firstWrong = -1;
  for (int cell = 0; cell < sudoku::kCells; ++cell) {
    if (game.value(cell) != 0) continue;
    const uint8_t right = game.solution(cell);
    if (wrongPlaced < 2) {
      game.place(cell, static_cast<uint8_t>(right % 9 + 1));
      if (firstWrong < 0) firstWrong = cell;
      ++wrongPlaced;
    } else {
      game.place(cell, right);
    }
  }
  EXPECT_EQ(game.emptyCells(), 0);
  EXPECT_FALSE(game.solved());
  EXPECT_EQ(game.wrongCells(), 2);

  EXPECT_EQ(game.clearWrong(), 2);
  EXPECT_EQ(game.wrongCells(), 0);
  EXPECT_EQ(game.emptyCells(), 2);
  EXPECT_EQ(game.value(firstWrong), 0);
  EXPECT_TRUE(game.undo());  // each cleared digit can come back
  EXPECT_EQ(game.wrongCells(), 1);

  for (int cell = 0; cell < sudoku::kCells; ++cell) {
    if (game.isWrong(cell) || game.value(cell) == 0) game.place(cell, game.solution(cell));
  }
  EXPECT_TRUE(game.solved());
  EXPECT_EQ(game.clearWrong(), 0);
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

  bytes[kValuesOffset] = 12;  // a cell value out of range
  sudoku::Game untouched;
  EXPECT_FALSE(untouched.load(bytes, sizeof(bytes)));
  EXPECT_FALSE(untouched.started());
  EXPECT_FALSE(untouched.load(bytes, 100));
}

TEST(SudokuGenerator, ExpertAndMasterGoBeyondTheHintRules) {
  int expertBeyond = 0;
  int masterBeyond = 0;
  int expertClues = 0;
  int masterClues = 0;
  for (uint32_t seed = 1; seed <= 10; ++seed) {
    for (const sudoku::Difficulty level : {sudoku::Difficulty::Expert, sudoku::Difficulty::Master}) {
      sudoku::Puzzle puzzle;
      sudoku::generate(level, seed * 2654435761u, puzzle);
      expectValidSolution(puzzle.solution);
      ASSERT_EQ(sudoku::countSolutions(puzzle.givens, 2), 1) << "seed " << seed;
      EXPECT_FALSE(sudoku::solvableBySingles(puzzle.givens)) << "seed " << seed;
      const bool beyond = !sudoku::solvableByRules(puzzle.givens);
      EXPECT_LE(clueCount(puzzle), 32) << "seed " << seed;
      if (level == sudoku::Difficulty::Expert) {
        expertClues += clueCount(puzzle);
        expertBeyond += beyond ? 1 : 0;
      } else {
        masterClues += clueCount(puzzle);
        masterBeyond += beyond ? 1 : 0;
      }
    }
  }
  EXPECT_GE(expertBeyond, 8);
  EXPECT_GE(masterBeyond, 8);
  EXPECT_LT(masterClues, expertClues - 10);  // Master keeps fewer clues
  printf("clues: Expert %.1f, Master %.1f on average\n", expertClues / 10.0, masterClues / 10.0);
}

TEST(SudokuHint, StepsAreRightAndSolveEasyToHard) {
  for (uint32_t seed = 1; seed <= 12; ++seed) {
    for (const sudoku::Difficulty level :
         {sudoku::Difficulty::Easy, sudoku::Difficulty::Medium, sudoku::Difficulty::Hard}) {
      sudoku::Puzzle puzzle;
      sudoku::generate(level, seed * 7919u, puzzle);
      uint8_t grid[sudoku::kCells];
      std::memcpy(grid, puzzle.givens, sizeof(grid));
      int shownOnly = 0;
      for (int steps = 0; steps < sudoku::kCells; ++steps) {
        const sudoku::Step step = sudoku::nextStep(grid, puzzle.solution, -1);
        if (step.cell < 0) break;
        ASSERT_NE(step.kind, sudoku::Step::Kind::Wrong);
        ASSERT_EQ(grid[step.cell], 0);
        ASSERT_EQ(step.digit, puzzle.solution[step.cell]) << "seed " << seed;
        if (step.kind == sudoku::Step::Kind::None) ++shownOnly;
        if (level != sudoku::Difficulty::Hard) {
          EXPECT_EQ(step.before, sudoku::Step::Rule::None);
        }
        grid[step.cell] = step.digit;
      }
      EXPECT_EQ(std::memcmp(grid, puzzle.solution, sizeof(grid)), 0);
      if (level != sudoku::Difficulty::Hard) {
        EXPECT_EQ(shownOnly, 0) << "seed " << seed;
      }
    }
  }
}

TEST(SudokuHint, NamesTheUnitOfAnOnlyPlaceAndFindsWrongDigitsFirst) {
  sudoku::Puzzle puzzle;
  sudoku::generate(sudoku::Difficulty::Medium, 77, puzzle);
  uint8_t grid[sudoku::kCells];
  std::memcpy(grid, puzzle.givens, sizeof(grid));

  sudoku::Step step = sudoku::nextStep(grid, puzzle.solution, -1);
  ASSERT_GE(step.cell, 0);
  if (step.kind == sudoku::Step::Kind::OnlyPlace) {
    // No other open cell of the unit can hold the digit.
    for (int i = 0; i < 9; ++i) {
      const int cell = sudoku::unitCell(step.unit, i);
      EXPECT_TRUE(cell == step.cell || grid[cell] != 0 || puzzle.solution[cell] != step.digit);
    }
  }

  // A cell the player chose is explained first when a rule reaches it.
  const int chosen = step.cell;
  const sudoku::Step forChosen = sudoku::nextStep(grid, puzzle.solution, chosen);
  EXPECT_EQ(forChosen.cell, chosen);

  int open = -1;
  for (int cell = sudoku::kCells - 1; cell >= 0 && open < 0; --cell) {
    if (grid[cell] == 0) open = cell;
  }
  grid[open] = static_cast<uint8_t>(puzzle.solution[open] % 9 + 1);
  step = sudoku::nextStep(grid, puzzle.solution, -1);
  EXPECT_EQ(step.kind, sudoku::Step::Kind::Wrong);
  EXPECT_EQ(step.cell, open);
  EXPECT_EQ(step.digit, grid[open]);
}

TEST(SudokuGame, DailyPuzzlesCountDaysInARow) {
  sudoku::Puzzle puzzle;
  sudoku::Game game;
  for (const uint16_t day : {100, 101, 102}) {
    sudoku::generate(sudoku::Difficulty::Medium, day, puzzle);
    game.start(puzzle, sudoku::Difficulty::Medium, day);
    EXPECT_EQ(game.dailyDay(), day);
    EXPECT_FALSE(game.dailySolved(day));
    solveRest(game);
    ASSERT_TRUE(game.solved());
    EXPECT_TRUE(game.dailySolved(day));
  }
  EXPECT_EQ(game.dailyStreak(102), 3);
  EXPECT_EQ(game.dailyStreak(103), 3);  // still alive until a day is missed
  EXPECT_EQ(game.dailyStreak(104), 0);
  EXPECT_EQ(game.dailyCount(), 3);

  // A day missed starts a new run; the longest is kept.
  game.start(puzzle, sudoku::Difficulty::Medium, 105);
  solveRest(game);
  EXPECT_EQ(game.dailyStreak(105), 1);
  EXPECT_EQ(game.bestDailyStreak(), 3);

  // An ordinary puzzle does not touch the run.
  game.start(puzzle, sudoku::Difficulty::Medium);
  solveRest(game);
  EXPECT_EQ(game.dailyStreak(105), 1);
  EXPECT_EQ(game.dailyCount(), 4);
  EXPECT_EQ(game.solvedCount(sudoku::Difficulty::Medium), 5);
}

TEST(SudokuGame, FillLastCellsChainsAndUndoesInOneStep) {
  sudoku::Puzzle puzzle;
  sudoku::generate(sudoku::Difficulty::Easy, 3, puzzle);
  // The solution with two cells of the first row open: one move leaves the last one.
  sudoku::Puzzle open = puzzle;
  std::memcpy(open.givens, puzzle.solution, sudoku::kCells);
  open.givens[0] = 0;
  open.givens[1] = 0;
  sudoku::Game game;
  game.start(open, sudoku::Difficulty::Easy);
  game.place(0, game.solution(0));
  EXPECT_EQ(game.fillLastCells(0), 1);
  EXPECT_TRUE(game.solved());

  // With another cell open elsewhere the grid stays unsolved, and one undo
  // takes back the fill and the move before it.
  open.givens[40] = 0;
  game.start(open, sudoku::Difficulty::Easy);
  game.place(0, game.solution(0));
  EXPECT_EQ(game.fillLastCells(0), 1);
  EXPECT_EQ(game.value(1), game.solution(1));
  EXPECT_FALSE(game.solved());
  EXPECT_TRUE(game.undo());
  EXPECT_EQ(game.value(0), 0);
  EXPECT_EQ(game.value(1), 0);
  EXPECT_FALSE(game.undo());
}

TEST(SudokuGame, RestartRepeatsAndExplainedHints) {
  sudoku::Puzzle puzzle;
  sudoku::generate(sudoku::Difficulty::Easy, 11, puzzle);
  sudoku::Game game;
  game.start(puzzle, sudoku::Difficulty::Easy);
  const int cell = firstOpenCell(game);
  // A digit already in the cell's row repeats there, and in the other cell too.
  int rowMate = -1;
  for (int i = 0; i < 9 && rowMate < 0; ++i) {
    const int other = (cell / 9) * 9 + i;
    if (other != cell && game.value(other) != 0) rowMate = other;
  }
  ASSERT_GE(rowMate, 0);
  game.place(cell, game.value(rowMate));
  EXPECT_TRUE(game.repeats(cell));
  EXPECT_TRUE(game.repeats(rowMate));
  EXPECT_EQ(game.mistakes(), 1);
  game.hint(-1);
  game.countHint();
  game.addElapsedSeconds(40);
  EXPECT_EQ(game.hintsUsed(), 2);

  game.restart();
  for (int i = 0; i < sudoku::kCells; ++i) {
    EXPECT_EQ(game.value(i), puzzle.givens[i]);
    EXPECT_FALSE(game.isHinted(i));
  }
  EXPECT_EQ(game.mistakes(), 0);
  EXPECT_EQ(game.hintsUsed(), 0);
  EXPECT_EQ(game.elapsedSeconds(), 0u);
  EXPECT_FALSE(game.repeats(cell));
}

TEST(SudokuGame, AveragesAndTheFirstSaveFormat) {
  sudoku::Puzzle puzzle;
  sudoku::generate(sudoku::Difficulty::Easy, 8, puzzle);
  sudoku::Game game;
  for (const uint32_t seconds : {100u, 200u}) {
    game.start(puzzle, sudoku::Difficulty::Easy);
    game.addElapsedSeconds(seconds);
    solveRest(game);
  }
  EXPECT_EQ(game.averageSeconds(sudoku::Difficulty::Easy), 150u);
  EXPECT_EQ(game.averageSeconds(sudoku::Difficulty::Hard), 0u);
  game.start(puzzle, sudoku::Difficulty::Easy);
  game.place(firstOpenCell(game), 5);

  // A file in the first format: the same layout up to the records of three levels.
  uint8_t current[sudoku::Game::kSaveBytes];
  game.save(current);
  uint8_t old[sudoku::Game::kSaveBytesV1] = {};
  std::memcpy(old, "VSD1", 4);
  std::memcpy(old + 4, current + 4, 12);    // state, mistakes, hints, time
  std::memcpy(old + 16, current + 16, 12);  // best times of Easy..Hard
  std::memcpy(old + 28, current + 36, 6);   // solved counts of Easy..Hard
  std::memcpy(old + 34, current + kValuesOffset, 405);
  sudoku::Game restored;
  ASSERT_TRUE(restored.load(old, sizeof(old)));
  EXPECT_EQ(restored.solvedCount(sudoku::Difficulty::Easy), 2);
  EXPECT_EQ(restored.bestSeconds(sudoku::Difficulty::Easy), 100u);
  EXPECT_EQ(restored.averageSeconds(sudoku::Difficulty::Easy), 0u);  // not kept back then
  EXPECT_EQ(restored.mistakes(), game.mistakes());
  for (int i = 0; i < sudoku::kCells; ++i) EXPECT_EQ(restored.value(i), game.value(i));
  old[6] = 3;  // the first format had no Expert
  EXPECT_FALSE(restored.load(old, sizeof(old)));
}
