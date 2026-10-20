#pragma once

#include <cstddef>
#include <cstdint>

// Tic Tac Toe on a 3 x 3 board, with a small computer opponent. Free of any
// device code so the host tests can play it.
//
// Classic: X starts, three in a row win, a full board without a row is a draw.
// Infinite: a side keeps only three marks. Placing a fourth takes that side's
// oldest mark off the board, so the board never fills and there is no draw.
// The mark that is about to leave still holds its cell for that move.
//
// Cells run 0..8, row by row from the top left.
namespace tictactoe {

enum Mark : uint8_t { None = 0, X = 1, O = 2 };
enum class Kind : uint8_t { Classic = 0, Infinite = 1 };
enum class State : uint8_t { NotStarted = 0, Playing = 1, XWins = 2, OWins = 3, Draw = 4 };

constexpr Mark other(const Mark mark) { return mark == X ? O : X; }

class Game {
 public:
  static constexpr size_t kSaveBytes = 12;

  void start(Kind kind);
  bool started() const { return state_ != State::NotStarted; }
  Kind kind() const { return kind_; }
  State state() const { return state_; }
  Mark toMove() const { return toMove_; }
  Mark at(const int cell) const { return static_cast<Mark>(board_[cell]); }
  // Infinite: the cell whose mark leaves with the next move; -1 when none does.
  int fading() const;
  // The cell of the newest mark; -1 on an empty board.
  int lastCell() const { return alive_ > 0 ? order_[alive_ - 1] : -1; }
  // The three cells of the winning row; false while nobody has won.
  bool winningCells(int cells[3]) const;
  bool play(int cell);

  // The best cell for the side to move, looking `depth` moves ahead; `seed`
  // chooses between equally good cells. -1 when the game is not being played.
  int bestMove(int depth, uint32_t seed) const;
  // How far the computer looks at a level (0 easy, 1 medium, 2 hard). Hard
  // plays a Classic game out to its end, so it cannot lose one.
  static int depthForLevel(int level, Kind kind);

  void save(uint8_t* out) const;
  bool load(const uint8_t* in, size_t size);

 private:
  int search(int depth, int alpha, int beta) const;

  Kind kind_ = Kind::Classic;
  State state_ = State::NotStarted;
  Mark toMove_ = X;
  uint8_t alive_ = 0;
  uint8_t order_[9] = {};  // the cells of the marks on the board, oldest first
  uint8_t board_[9] = {};
};

}  // namespace tictactoe
