#pragma once

#include <cstddef>
#include <cstdint>

// Go (weiqi, baduk) on 9x9, 13x13 or 19x19, with a small computer opponent.
// Free of any device code so the host tests can play it.
//
// The rules: Black moves first; a stone or chain without liberties is
// captured; a move that would leave its own chain without liberties is not
// allowed (no suicide) unless it captures; a single-stone capture may not be
// answered at once by retaking the single stone (ko). Two passes in a row end
// the play; the players then mark the dead stones and the score is counted by
// area (stones on the board + surrounded empty points), White getting 6.5
// points of komi.
//
// Points run 0..size*size-1, row by row from the top.
namespace go {

enum Stone : uint8_t { Empty = 0, Black = 1, White = 2 };
constexpr Stone other(const Stone stone) { return stone == Black ? White : Black; }

constexpr int kMaxSize = 19;
constexpr int kMaxPoints = kMaxSize * kMaxSize;
constexpr int kPass = -1;
constexpr int kKomiTenths = 65;

struct Board {
  uint8_t size = 9;
  uint8_t cell[kMaxPoints] = {};
  uint8_t toMove = Black;
  int16_t ko = -1;                 // the point the side to move may not play (ko), or -1
  uint16_t captured[3] = {0, 0, 0};  // stones taken by Black [1] and by White [2]

  void reset(uint8_t boardSize);
  int points() const { return size * size; }
  // The neighbours of a point on the board. Returns their number.
  int neighbours(int point, int out[4]) const;
  // The liberties of the chain on `point`; stones (optional) gets its points.
  int liberties(int point, int16_t* stones = nullptr, int* stoneCount = nullptr) const;
  bool legal(int point) const;
  // Plays a stone for the side to move. False (and nothing changes) when illegal.
  bool play(int point);
  void pass();
};

struct Score {
  int blackTenths = 0;
  int whiteTenths = 0;  // komi included
};

// Area scoring. dead (may be null) marks points whose stones are dead: they
// count as captured and their points as the other side's area.
Score score(const Board& board, const uint8_t* deadBits, int komiTenths = kKomiTenths);

// The computer's move for the side to move: kPass when it has nothing useful
// left. level 0 is Easy (plays loosely), 1 is Medium. opponentPassed lets it
// pass back once the game is settled.
int chooseMove(const Board& board, int level, bool opponentPassed, uint32_t seed);

class Game {
 public:
  enum class State : uint8_t { NotStarted, Playing, MarkingDead, Finished };

  static constexpr int kMaxMoves = 720;
  static constexpr int kDeadBytes = (kMaxPoints + 7) / 8;
  static constexpr size_t kSaveBytes = 4 + kDeadBytes + kMaxMoves * 2;

  void start(uint8_t size);
  State state() const { return state_; }
  bool started() const { return state_ != State::NotStarted; }
  const Board& board() const { return board_; }
  uint8_t size() const { return board_.size; }
  Stone at(int point) const { return static_cast<Stone>(board_.cell[point]); }
  Stone toMove() const { return static_cast<Stone>(board_.toMove); }
  int lastMove() const { return moveCount_ > 0 ? moves_[moveCount_ - 1] : kPass; }
  bool lastWasPass() const { return moveCount_ > 0 && moves_[moveCount_ - 1] == kPass; }
  int moveCount() const { return moveCount_; }

  bool play(int point);
  // Two passes in a row end the play: the dead stones are marked next.
  void pass();
  // While marking: a tap on a chain marks it dead, or alive again.
  void toggleDead(int point);
  bool isDead(int point) const { return (dead_[point >> 3] >> (point & 7)) & 1; }
  const uint8_t* deadBits() const { return dead_; }
  // The players agree: the score is final.
  void finish() { state_ = State::Finished; }
  // Back to playing from marking (the passes are taken back).
  void resume();
  // Takes back the last move (a pass included).
  bool undo();
  bool canUndo() const { return moveCount_ > 0; }
  Score result() const { return score(board_, dead_); }

  size_t save(uint8_t out[kSaveBytes]) const;
  bool load(const uint8_t* data, size_t size);

 private:
  void replay(int count);

  Board board_;
  State state_ = State::NotStarted;
  int16_t moves_[kMaxMoves] = {};
  int moveCount_ = 0;
  uint8_t dead_[kDeadBytes] = {};
};

}  // namespace go
