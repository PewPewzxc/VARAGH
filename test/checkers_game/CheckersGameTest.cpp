#include <gtest/gtest.h>

#include <cstring>

#include "src/games/CheckersGame.h"

namespace {

using checkers::Dark;
using checkers::King;
using checkers::kLightBit;
using checkers::Light;
using checkers::Man;

int sq(const int row, const int col) { return row * 8 + col; }

checkers::Position emptyBoard(const checkers::Side side) {
  checkers::Position position;
  memset(position.board, 0, sizeof(position.board));
  position.side = side;
  return position;
}

bool hasStep(const checkers::Step* steps, const int count, const int from, const int to) {
  for (int i = 0; i < count; ++i) {
    if (steps[i].from == from && steps[i].to == to) return true;
  }
  return false;
}

}  // namespace

TEST(CheckersRules, StartingPosition) {
  checkers::Position position;
  position.setStart();
  EXPECT_EQ(position.count(Dark), 12);
  EXPECT_EQ(position.count(Light), 12);
  EXPECT_EQ(position.side, Dark);
  for (int square = 0; square < 64; ++square) {
    if (!checkers::playable(square)) {
      EXPECT_EQ(position.board[square], 0);
    }
  }
  checkers::Step steps[checkers::kMaxSteps];
  EXPECT_EQ(checkers::generateSteps(position, steps), 7);
}

TEST(CheckersRules, MenMoveForwardsOnlyKingsBothWays) {
  checkers::Position position = emptyBoard(Light);
  position.board[sq(4, 3)] = Man | kLightBit;
  position.board[sq(0, 1)] = Man;
  checkers::Step steps[checkers::kMaxSteps];
  int count = checkers::generateSteps(position, steps);
  EXPECT_EQ(count, 2);
  EXPECT_TRUE(hasStep(steps, count, sq(4, 3), sq(3, 2)));
  EXPECT_TRUE(hasStep(steps, count, sq(4, 3), sq(3, 4)));

  position.board[sq(4, 3)] = King | kLightBit;
  count = checkers::generateSteps(position, steps);
  EXPECT_EQ(count, 4);
  EXPECT_TRUE(hasStep(steps, count, sq(4, 3), sq(5, 2)));
}

TEST(CheckersRules, ACaptureMustBeTaken) {
  checkers::Position position = emptyBoard(Light);
  position.board[sq(4, 5)] = Man | kLightBit;
  position.board[sq(6, 1)] = Man | kLightBit;
  position.board[sq(3, 4)] = Man;
  checkers::Step steps[checkers::kMaxSteps];
  const int count = checkers::generateSteps(position, steps);
  ASSERT_EQ(count, 1);
  EXPECT_EQ(steps[0].from, sq(4, 5));
  EXPECT_EQ(steps[0].to, sq(2, 3));
  EXPECT_EQ(steps[0].captured, sq(3, 4));
  checkers::applyStep(position, steps[0]);
  EXPECT_EQ(position.board[sq(3, 4)], 0);
  EXPECT_EQ(position.side, Dark);
  EXPECT_EQ(position.count(Dark), 0);
}

TEST(CheckersRules, MenDoNotCaptureBackwards) {
  checkers::Position position = emptyBoard(Light);
  position.board[sq(3, 4)] = Man | kLightBit;
  position.board[sq(4, 5)] = Man;  // behind the light man
  checkers::Step steps[checkers::kMaxSteps];
  const int count = checkers::generateSteps(position, steps);
  for (int i = 0; i < count; ++i) EXPECT_EQ(steps[i].captured, checkers::kNone);
}

TEST(CheckersRules, AMultiJumpKeepsTheTurn) {
  checkers::Position position = emptyBoard(Light);
  position.board[sq(6, 1)] = Man | kLightBit;
  position.board[sq(5, 2)] = Man;
  position.board[sq(3, 4)] = Man;
  position.board[sq(0, 7)] = Man;
  checkers::Step steps[checkers::kMaxSteps];
  ASSERT_EQ(checkers::generateSteps(position, steps), 1);
  checkers::applyStep(position, steps[0]);
  EXPECT_EQ(position.side, Light);
  EXPECT_EQ(position.jumper, sq(4, 3));
  ASSERT_EQ(checkers::generateSteps(position, steps), 1);
  EXPECT_EQ(steps[0].to, sq(2, 5));
  checkers::applyStep(position, steps[0]);
  EXPECT_EQ(position.side, Dark);
  EXPECT_EQ(position.jumper, checkers::kNone);
  EXPECT_EQ(position.count(Dark), 1);
}

TEST(CheckersRules, BeingCrownedEndsTheMove) {
  checkers::Position position = emptyBoard(Light);
  position.board[sq(2, 1)] = Man | kLightBit;
  position.board[sq(1, 2)] = Man;
  position.board[sq(1, 4)] = Man;  // a king on (0,3) could jump this one
  checkers::Step steps[checkers::kMaxSteps];
  ASSERT_EQ(checkers::generateSteps(position, steps), 1);
  checkers::applyStep(position, steps[0]);
  EXPECT_TRUE(checkers::isKing(position.board[sq(0, 3)]));
  EXPECT_EQ(position.side, Dark);
  EXPECT_EQ(position.jumper, checkers::kNone);
}

TEST(CheckersGame, PlayUndoAndForcedCaptureFlag) {
  checkers::Game game;
  game.start();
  EXPECT_FALSE(game.mustCapture());
  uint8_t squares[32];
  EXPECT_EQ(game.movable(squares), 4);
  EXPECT_FALSE(game.play(sq(5, 0), sq(4, 1)));  // Light may not start
  ASSERT_TRUE(game.play(sq(2, 1), sq(3, 2)));
  ASSERT_TRUE(game.play(sq(5, 4), sq(4, 3)));
  EXPECT_TRUE(game.mustCapture());
  EXPECT_EQ(game.targets(sq(3, 2), squares), 1);
  ASSERT_TRUE(game.play(sq(3, 2), sq(5, 4)));
  EXPECT_EQ(game.position().count(Light), 11);

  // Back to where Dark was about to capture.
  ASSERT_TRUE(game.undoTo(Dark));
  EXPECT_EQ(game.position().count(Light), 12);
  EXPECT_EQ(game.sideToMove(), Dark);
  EXPECT_TRUE(game.mustCapture());
}

TEST(CheckersGame, LoadRejectsADamagedSave) {
  checkers::Game game;
  game.start();
  uint8_t bytes[checkers::Game::kSaveBytes];
  game.save(bytes);
  checkers::Game loaded;
  ASSERT_TRUE(loaded.load(bytes, sizeof(bytes)));
  EXPECT_EQ(loaded.state(), checkers::Game::State::Playing);
  EXPECT_EQ(loaded.position().count(Dark), 12);
  bytes[1] = 'x';
  EXPECT_FALSE(loaded.load(bytes, sizeof(bytes)));
  EXPECT_FALSE(loaded.started());
}

TEST(CheckersGame, SaveKeepsTheUndoHistory) {
  checkers::Game game;
  game.start();
  ASSERT_TRUE(game.play(sq(2, 1), sq(3, 2)));
  ASSERT_TRUE(game.play(sq(5, 2), sq(4, 1)));
  uint8_t bytes[checkers::Game::kSaveBytes];
  game.save(bytes);
  checkers::Game loaded;
  ASSERT_TRUE(loaded.load(bytes, sizeof(bytes)));
  EXPECT_TRUE(loaded.position() == game.position());
  ASSERT_TRUE(loaded.undoTo(Dark));
  checkers::Position start;
  start.setStart();
  EXPECT_TRUE(loaded.position() == start);
}

TEST(CheckersComputer, TakesTheBetterCapture) {
  checkers::Position position = emptyBoard(Light);
  // One capture wins a single piece, the other leads to a second jump.
  position.board[sq(6, 1)] = Man | kLightBit;
  position.board[sq(5, 2)] = Man;
  position.board[sq(3, 2)] = Man;
  position.board[sq(6, 7)] = Man | kLightBit;
  position.board[sq(5, 6)] = Man;
  position.board[sq(0, 1)] = Man;
  checkers::SearchOptions options;
  options.maxDepth = 4;
  const checkers::SearchResult result = checkers::search(position, options);
  ASSERT_TRUE(result.found);
  EXPECT_EQ(result.best.from, sq(6, 1));
}

TEST(CheckersComputer, PlaysWholeGamesLegally) {
  for (uint32_t seed = 1; seed <= 3; ++seed) {
    checkers::Game game;
    game.start();
    int steps = 0;
    while (game.state() == checkers::Game::State::Playing && steps < 400) {
      checkers::SearchOptions options;
      options.maxDepth = 3;
      options.margin = 30;
      options.seed = seed * 104729 + static_cast<uint32_t>(steps);
      const checkers::SearchResult result = checkers::search(game.position(), options);
      ASSERT_TRUE(result.found);
      ASSERT_TRUE(game.play(result.best)) << "illegal step " << steps;
      ++steps;
    }
    EXPECT_GT(steps, 20);
    EXPECT_NE(game.state(), checkers::Game::State::Playing);
  }
}

TEST(CheckersComputer, StopsAtItsNodeLimit) {
  checkers::Position position;
  position.setStart();
  checkers::SearchOptions options;
  options.maxDepth = 30;
  options.maxNodes = 20000;
  const checkers::SearchResult result = checkers::search(position, options);
  ASSERT_TRUE(result.found);
  EXPECT_GE(result.depth, 1);
  EXPECT_LT(result.nodes, 30000u);
}
