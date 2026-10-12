#include <gtest/gtest.h>

#include <cstring>
#include <vector>

#include "src/games/ChessGame.h"

namespace {

uint64_t perftOf(const char* fen, const int depth) {
  chess::Position position;
  EXPECT_TRUE(position.setFen(fen));
  return chess::perft(position, depth);
}

int square(const char* name) { return (name[1] - '1') * 8 + (name[0] - 'a'); }

bool playAll(chess::Game& game, std::initializer_list<const char*> moves) {
  for (const char* move : moves) {
    if (!game.play(square(move), square(move + 2))) return false;
  }
  return true;
}

}  // namespace

// The standard move-generator check: the number of move sequences from well
// known positions must match the published counts exactly. Together these
// positions cover castling, en passant, promotion, pins and checks.
TEST(ChessRules, PerftFromTheStartingPosition) {
  const char* start = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq -";
  EXPECT_EQ(perftOf(start, 1), 20u);
  EXPECT_EQ(perftOf(start, 2), 400u);
  EXPECT_EQ(perftOf(start, 3), 8902u);
  EXPECT_EQ(perftOf(start, 4), 197281u);
}

TEST(ChessRules, PerftOfTheCastlingAndPinPosition) {
  const char* fen = "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq -";
  EXPECT_EQ(perftOf(fen, 1), 48u);
  EXPECT_EQ(perftOf(fen, 2), 2039u);
  EXPECT_EQ(perftOf(fen, 3), 97862u);
}

TEST(ChessRules, PerftOfTheEnPassantEndgame) {
  const char* fen = "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - -";
  EXPECT_EQ(perftOf(fen, 1), 14u);
  EXPECT_EQ(perftOf(fen, 2), 191u);
  EXPECT_EQ(perftOf(fen, 3), 2812u);
  EXPECT_EQ(perftOf(fen, 4), 43238u);
}

TEST(ChessRules, PerftOfThePromotionPositions) {
  EXPECT_EQ(perftOf("r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq -", 3), 9467u);
  EXPECT_EQ(perftOf("rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ -", 3), 62379u);
}

TEST(ChessRules, MakingAndTakingBackAMoveRestoresThePosition) {
  chess::Position position;
  ASSERT_TRUE(position.setFen("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq -"));
  const chess::Position before = position;
  chess::Move moves[chess::kMaxMoves];
  const int count = chess::generateLegal(position, moves);
  for (int i = 0; i < count; ++i) {
    chess::Undo undo;
    chess::makeMove(position, moves[i], undo);
    chess::Position check = position;
    check.recompute();
    EXPECT_EQ(check.hash, position.hash) << "hash drifted on move " << i;
    EXPECT_EQ(check.lock, position.lock) << "lock drifted on move " << i;
    EXPECT_EQ(check.placed, position.placed) << "score drifted on move " << i;
    EXPECT_EQ(check.heavy[chess::White], position.heavy[chess::White]);
    EXPECT_EQ(check.heavy[chess::Black], position.heavy[chess::Black]);
    chess::unmakeMove(position, moves[i], undo);
    EXPECT_EQ(memcmp(position.board, before.board, sizeof(before.board)), 0);
    EXPECT_EQ(position.hash, before.hash);
    EXPECT_EQ(position.lock, before.lock);
    EXPECT_EQ(position.placed, before.placed);
    EXPECT_EQ(position.castling, before.castling);
  }
}

// The scores a position carries along are only ever changed move by move: over
// whole games of every kind of move they must stay what the board says.
TEST(ChessRules, CarriedScoresMatchTheBoardThroughWholeGames) {
  const char* const starts[] = {
      "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq -",
      "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq -",
      "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq -",
      "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - -",
  };
  uint32_t random = 2463534242u;
  for (const char* start : starts) {
    for (int game = 0; game < 40; ++game) {
      chess::Position position;
      ASSERT_TRUE(position.setFen(start));
      for (int ply = 0; ply < 120; ++ply) {
        chess::Move moves[chess::kMaxMoves];
        const int count = chess::generateLegal(position, moves);
        if (count == 0) break;
        random ^= random << 13;
        random ^= random >> 17;
        random ^= random << 5;
        chess::Undo undo;
        chess::makeMove(position, moves[random % static_cast<uint32_t>(count)], undo);
        chess::Position check = position;
        check.recompute();
        ASSERT_EQ(check.hash, position.hash);
        ASSERT_EQ(check.lock, position.lock);
        ASSERT_EQ(check.placed, position.placed);
        ASSERT_EQ(check.heavy[chess::White], position.heavy[chess::White]);
        ASSERT_EQ(check.heavy[chess::Black], position.heavy[chess::Black]);
        ASSERT_EQ(chess::evaluate(check), chess::evaluate(position));
      }
    }
  }
}

TEST(ChessGame, FoolsMateIsCheckmate) {
  chess::Game game;
  game.start();
  ASSERT_TRUE(playAll(game, {"f2f3", "e7e5", "g2g4", "d8h4"}));
  EXPECT_EQ(game.state(), chess::Game::State::Checkmate);
  EXPECT_EQ(game.sideToMove(), chess::White);
  EXPECT_FALSE(game.play(square("a2"), square("a3")));
}

TEST(ChessGame, IllegalMovesAreRefused) {
  chess::Game game;
  game.start();
  EXPECT_FALSE(game.play(square("e2"), square("e5")));
  EXPECT_FALSE(game.play(square("e7"), square("e5")));  // not Black's turn
  EXPECT_TRUE(game.play(square("e2"), square("e4")));
  uint8_t targets[32];
  // The king's knight has two squares at the start.
  EXPECT_EQ(game.targets(square("g8"), targets), 2);
}

TEST(ChessGame, UndoGoesBackOneMoveAtATime) {
  chess::Game game;
  game.start();
  ASSERT_TRUE(playAll(game, {"e2e4", "e7e5", "g1f3"}));
  ASSERT_TRUE(game.undo());
  EXPECT_EQ(game.plies(), 2);
  EXPECT_EQ(game.at(square("g1")), chess::Knight);
  EXPECT_EQ(game.at(square("f3")), 0);
  EXPECT_EQ(game.sideToMove(), chess::White);
  ASSERT_TRUE(game.undo());
  ASSERT_TRUE(game.undo());
  EXPECT_FALSE(game.undo());
}

TEST(ChessGame, CastlingMovesTheRookAndCapturesAreCounted) {
  chess::Game game;
  game.start();
  ASSERT_TRUE(playAll(game, {"e2e4", "e7e5", "g1f3", "b8c6", "f1b5", "a7a6", "b5c6", "d7c6", "e1g1"}));
  EXPECT_EQ(chess::pieceOf(game.at(square("g1"))), chess::King);
  EXPECT_EQ(chess::pieceOf(game.at(square("f1"))), chess::Rook);
  EXPECT_EQ(game.at(square("h1")), 0);
  EXPECT_EQ(game.lost(chess::Black, chess::Knight), 1);
  EXPECT_EQ(game.lost(chess::White, chess::Bishop), 1);
  EXPECT_EQ(game.lost(chess::White, chess::Pawn), 0);
}

TEST(ChessGame, SaveAndLoadKeepTheGame) {
  chess::Game game;
  game.start();
  ASSERT_TRUE(playAll(game, {"d2d4", "d7d5", "c2c4", "d5c4"}));
  uint8_t bytes[chess::Game::kSaveBytes];
  game.save(bytes);
  chess::Game loaded;
  ASSERT_TRUE(loaded.load(bytes, sizeof(bytes)));
  EXPECT_EQ(loaded.plies(), 4);
  EXPECT_EQ(loaded.position().hash, game.position().hash);
  EXPECT_EQ(loaded.lastMove().toSquare(), square("c4"));
  bytes[0] = 'X';
  EXPECT_FALSE(loaded.load(bytes, sizeof(bytes)));
  EXPECT_FALSE(loaded.started());
}

TEST(ChessGame, ThreefoldRepetitionIsADraw) {
  chess::Game game;
  game.start();
  ASSERT_TRUE(playAll(game, {"g1f3", "g8f6", "f3g1", "f6g8", "g1f3", "g8f6", "f3g1"}));
  EXPECT_EQ(game.state(), chess::Game::State::Playing);
  ASSERT_TRUE(game.play(square("f6"), square("g8")));
  EXPECT_EQ(game.state(), chess::Game::State::DrawRepetition);
}

TEST(ChessComputer, TakesAFreeQueen) {
  chess::Position position;
  ASSERT_TRUE(position.setFen("4k3/8/8/8/2q5/8/4B3/4K3 w - -"));
  chess::SearchOptions options;
  options.maxDepth = 3;
  const chess::SearchResult result = chess::search(position, options, nullptr, 0);
  ASSERT_TRUE(result.found);
  EXPECT_EQ(result.best.fromSquare(), square("e2"));
  EXPECT_EQ(result.best.toSquare(), square("c4"));
}

TEST(ChessComputer, FindsMateInOne) {
  chess::Position position;
  // Back-rank mate: Ra8#.
  ASSERT_TRUE(position.setFen("6k1/5ppp/8/8/8/8/8/R3K3 w - -"));
  chess::SearchOptions options;
  options.maxDepth = 3;
  const chess::SearchResult result = chess::search(position, options, nullptr, 0);
  ASSERT_TRUE(result.found);
  EXPECT_EQ(result.best.fromSquare(), square("a1"));
  EXPECT_EQ(result.best.toSquare(), square("a8"));
  EXPECT_GT(result.score, chess::kMateScore - 100);
}

TEST(ChessComputer, FindsMateInTwo) {
  chess::Position position;
  // Two rooks against a bare king on the edge: 1.Ra7 and 2.Rb8#.
  ASSERT_TRUE(position.setFen("7k/8/8/8/8/8/R7/1R2K3 w - -"));
  chess::SearchOptions options;
  options.maxDepth = 4;
  const chess::SearchResult result = chess::search(position, options, nullptr, 0);
  ASSERT_TRUE(result.found);
  EXPECT_GT(result.score, chess::kMateScore - 100);
}

TEST(ChessComputer, StopsWhenTold) {
  chess::Position position;
  position.setStart();
  chess::SearchOptions options;
  options.maxDepth = 30;
  options.maxNodes = 20000;
  const chess::SearchResult result = chess::search(position, options, nullptr, 0);
  ASSERT_TRUE(result.found);
  EXPECT_GE(result.depth, 1);
  EXPECT_LT(result.nodes, 30000u);
}

// The memory of examined positions must never change what is found, only how
// soon: the same mates, with and without it, and from a table already filled
// by another position's search.
TEST(ChessComputer, RememberingPositionsKeepsTheAnswers) {
  std::vector<uint8_t> table(4096 * chess::kTableEntryBytes, 0);
  struct Case {
    const char* fen;
    int depth;
    bool mate;
  };
  const Case cases[] = {
      {"6k1/5ppp/8/8/8/8/8/R3K3 w - -", 3, true},
      {"4k3/8/8/8/2q5/8/4B3/4K3 w - -", 3, false},
      // Smothered mate, the queen given up first: Qg8+ Rxg8, Nf7#.
      {"5r1k/6pp/7N/8/2Q5/8/8/6K1 w - -", 4, true},
      {"7k/8/8/8/8/8/R7/1R2K3 w - -", 4, true},
  };
  // Twice over, so the second round starts from a table the others have filled.
  for (int round = 0; round < 2; ++round) {
    for (const Case& test : cases) {
      chess::Position position;
      ASSERT_TRUE(position.setFen(test.fen));
      chess::SearchOptions plain;
      plain.maxDepth = test.depth;
      const chess::SearchResult without = chess::search(position, plain, nullptr, 0);
      chess::SearchOptions remembering = plain;
      remembering.table = table.data();
      remembering.tableBytes = table.size();
      const chess::SearchResult with = chess::search(position, remembering, nullptr, 0);
      ASSERT_TRUE(without.found);
      ASSERT_TRUE(with.found);
      if (test.mate) {
        EXPECT_GT(without.score, chess::kMateScore - 100) << test.fen;
        EXPECT_EQ(with.score, without.score) << test.fen;
      } else {
        EXPECT_TRUE(with.best.sameAs(without.best)) << test.fen;
      }
    }
  }
}

TEST(ChessComputer, RememberingPositionsLooksAtFewerOfThem) {
  chess::Position position;
  ASSERT_TRUE(position.setFen("r1bqkbnr/pppp1ppp/2n5/4p3/4P3/5N2/PPPP1PPP/RNBQKB1R w KQkq -"));
  chess::SearchOptions plain;
  plain.maxDepth = 5;
  const chess::SearchResult without = chess::search(position, plain, nullptr, 0);
  std::vector<uint8_t> table(65536 * chess::kTableEntryBytes, 0);
  chess::SearchOptions remembering = plain;
  remembering.table = table.data();
  remembering.tableBytes = table.size();
  const chess::SearchResult with = chess::search(position, remembering, nullptr, 0);
  ASSERT_TRUE(without.found);
  ASSERT_TRUE(with.found);
  EXPECT_EQ(with.depth, without.depth);
  EXPECT_LT(with.nodes, without.nodes);
}

// Two computers play each other with the Easy settings: every move must be
// legal and the game must end or reach the move limit without a fault.
TEST(ChessComputer, PlaysWholeGamesLegally) {
  for (uint32_t seed = 1; seed <= 3; ++seed) {
    chess::Game game;
    game.start();
    int moves = 0;
    while (game.state() == chess::Game::State::Playing && moves < 160) {
      chess::SearchOptions options;
      options.maxDepth = 2;
      options.margin = 60;
      options.seed = seed * 7919 + static_cast<uint32_t>(moves);
      const chess::SearchResult result =
          chess::search(game.position(), options, game.hashes(), game.hashCount());
      ASSERT_TRUE(result.found);
      ASSERT_TRUE(game.play(result.best)) << "illegal move at ply " << moves;
      ++moves;
    }
    EXPECT_GT(moves, 10);
  }
}
