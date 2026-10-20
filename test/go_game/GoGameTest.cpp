#include "src/games/GoGame.h"

#include <gtest/gtest.h>

#include <string>

namespace {

int at(const int size, const int row, const int column) { return row * size + column; }

// Lays out a 9x9 board from text ("X" black, "O" white, "." empty), Black to move.
go::Board boardFrom(const char* const rows[9], const go::Stone toMove = go::Black) {
  go::Board board;
  board.reset(9);
  for (int r = 0; r < 9; ++r) {
    for (int c = 0; c < 9; ++c) {
      const char ch = rows[r][c];
      board.cell[at(9, r, c)] = ch == 'X' ? go::Black : ch == 'O' ? go::White : go::Empty;
    }
  }
  board.toMove = toMove;
  return board;
}

}  // namespace

TEST(GoGame, CapturesAStoneWithoutLiberties) {
  const char* rows[9] = {".........", "....X....", "...XO....", ".........", ".........",
                         ".........", ".........", ".........", "........."};
  go::Board board = boardFrom(rows);
  ASSERT_TRUE(board.play(at(9, 2, 5)));   // X at the right of O
  board.pass();                          // Black again
  ASSERT_TRUE(board.play(at(9, 3, 4)));   // last liberty
  EXPECT_EQ(board.cell[at(9, 2, 4)], go::Empty);
  EXPECT_EQ(board.captured[go::Black], 1);
}

TEST(GoGame, CapturesAWholeChain) {
  const char* rows[9] = {"XOO......", "XOO......", "XXX......", ".........", ".........",
                         ".........", ".........", ".........", "........."};
  go::Board board = boardFrom(rows);
  ASSERT_TRUE(board.play(at(9, 0, 3)));
  board.pass();
  ASSERT_TRUE(board.play(at(9, 1, 3)));
  for (const int p : {at(9, 0, 1), at(9, 0, 2), at(9, 1, 1), at(9, 1, 2)}) EXPECT_EQ(board.cell[p], go::Empty);
  EXPECT_EQ(board.captured[go::Black], 4);
}

TEST(GoGame, NoSuicide) {
  const char* rows[9] = {".X.......", "X........", ".........", ".........", ".........",
                         ".........", ".........", ".........", "........."};
  go::Board board = boardFrom(rows, go::White);
  EXPECT_FALSE(board.legal(at(9, 0, 0)));
  EXPECT_FALSE(board.play(at(9, 0, 0)));
  EXPECT_EQ(board.toMove, go::White);
}

TEST(GoGame, SuicideThatCapturesIsAllowed) {
  // White at the corner takes the black stone at (0,1) by filling its last liberty.
  const char* rows[9] = {".XO......", "XO.......", "O........", ".........", ".........",
                         ".........", ".........", ".........", "........."};
  go::Board board = boardFrom(rows, go::White);
  EXPECT_TRUE(board.play(at(9, 0, 0)));
  EXPECT_EQ(board.cell[at(9, 0, 1)], go::Empty);
}

TEST(GoGame, KoMayNotBeRetakenAtOnce) {
  const char* rows[9] = {".........", "..XO.....", ".X.XO....", "..XO.....", ".........",
                         ".........", ".........", ".........", "........."};
  // White plays into the gap at (2,2) and captures the black stone at (2,3).
  go::Board board = boardFrom(rows, go::White);
  ASSERT_TRUE(board.play(at(9, 2, 2)));
  EXPECT_EQ(board.cell[at(9, 2, 3)], go::Empty);
  // Black may not retake at (2,3) straight away...
  EXPECT_FALSE(board.legal(at(9, 2, 3)));
  // ...but may after playing elsewhere and White answering.
  ASSERT_TRUE(board.play(at(9, 7, 7)));
  ASSERT_TRUE(board.play(at(9, 7, 1)));
  EXPECT_TRUE(board.legal(at(9, 2, 3)));
}

TEST(GoGame, TwoPassesEndThePlayAndAreaIsCounted) {
  go::Game game;
  game.start(9);
  // Black walls off the left four columns, White the right four.
  for (int r = 0; r < 9; ++r) {
    ASSERT_TRUE(game.play(at(9, r, 3)));
    ASSERT_TRUE(game.play(at(9, r, 5)));
  }
  game.pass();
  EXPECT_EQ(game.state(), go::Game::State::Playing);
  game.pass();
  EXPECT_EQ(game.state(), go::Game::State::MarkingDead);
  const go::Score score = game.result();
  // Black: 9 stones + 27 points; White: 9 stones + 27 points + 6.5 komi. Column 4 is neutral.
  EXPECT_EQ(score.blackTenths, 360);
  EXPECT_EQ(score.whiteTenths, 425);
}

TEST(GoGame, DeadStonesCountForTheOtherSide) {
  go::Game game;
  game.start(9);
  for (int r = 0; r < 9; ++r) {
    ASSERT_TRUE(game.play(at(9, r, 3)));
    ASSERT_TRUE(game.play(at(9, r, 5)));
  }
  ASSERT_TRUE(game.play(at(9, 4, 7)));  // a black stone inside White's area
  game.pass();
  game.pass();
  ASSERT_EQ(game.state(), go::Game::State::MarkingDead);
  game.toggleDead(at(9, 4, 7));
  EXPECT_TRUE(game.isDead(at(9, 4, 7)));
  const go::Score score = game.result();
  EXPECT_EQ(score.whiteTenths, 425);
  EXPECT_EQ(score.blackTenths, 360);
  game.toggleDead(at(9, 4, 7));
  EXPECT_FALSE(game.isDead(at(9, 4, 7)));
}

TEST(GoGame, UndoAndSaveRoundTrip) {
  go::Game game;
  game.start(13);
  ASSERT_TRUE(game.play(at(13, 3, 3)));
  ASSERT_TRUE(game.play(at(13, 9, 9)));
  ASSERT_TRUE(game.undo());
  EXPECT_EQ(game.at(at(13, 9, 9)), go::Empty);
  EXPECT_EQ(game.toMove(), go::White);
  ASSERT_TRUE(game.play(at(13, 9, 3)));
  game.pass();

  uint8_t bytes[go::Game::kSaveBytes];
  game.save(bytes);
  go::Game loaded;
  ASSERT_TRUE(loaded.load(bytes, sizeof(bytes)));
  EXPECT_EQ(loaded.size(), 13);
  EXPECT_EQ(loaded.moveCount(), 3);
  EXPECT_EQ(loaded.at(at(13, 3, 3)), go::Black);
  EXPECT_EQ(loaded.at(at(13, 9, 3)), go::White);
  EXPECT_TRUE(loaded.lastWasPass());
}

TEST(GoGame, ComputerTakesACaptureAndNeverFillsItsEye) {
  const char* rows[9] = {".........", "....X....", "...XO....", "....X....", ".........",
                         ".........", ".........", ".........", "........."};
  const go::Board board = boardFrom(rows);
  EXPECT_EQ(go::chooseMove(board, 1, false, 7), at(9, 2, 5));

  // Black's only empty points are its two eyes: it passes rather than fill one.
  const char* eyes[9] = {"X.XOOOOOO", "XXXOOOOOO", "X.XOOOOOO", "XXXOOOOOO", "OOOOOOOOO",
                         "OOOOOOOOO", "OOOOOOOOO", "OOOOOOOOO", "OOOOOOOO."};
  const go::Board eyed = boardFrom(eyes);
  const int move = go::chooseMove(eyed, 1, false, 7);
  EXPECT_NE(move, at(9, 0, 1));
  EXPECT_NE(move, at(9, 2, 1));
}

TEST(GoGame, ComputerPlaysLegalMovesThroughAWholeGame) {
  go::Game game;
  game.start(9);
  uint32_t seed = 11;
  for (int turn = 0; turn < 400 && game.state() == go::Game::State::Playing; ++turn) {
    const int move = go::chooseMove(game.board(), turn % 2, game.lastWasPass(), seed += 7919);
    if (move == go::kPass) {
      game.pass();
    } else {
      ASSERT_TRUE(game.play(move)) << "turn " << turn;
    }
  }
  EXPECT_NE(game.state(), go::Game::State::Playing);
}
