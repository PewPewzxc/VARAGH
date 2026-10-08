#pragma once

#include <cstddef>
#include <cstdint>

// Checkers by the American / English rules, with a small computer opponent.
// Free of any device code so the host tests can play it.
//
// The rules: pieces move one square diagonally forwards and capture by jumping
// forwards over an adjacent enemy piece; kings do both in every direction, one
// square at a time. A capture must be taken, and a piece that can jump again
// must go on. A piece reaching the far row is crowned and its move ends there.
// Dark moves first; a side that cannot move has lost.
//
// Squares run 0..63, row by row from the top of the board as it is stored:
// Dark starts on the top three rows and moves down, Light starts at the bottom.
namespace checkers {

enum Side : uint8_t { Dark = 0, Light = 1 };

// A square's content.
enum : uint8_t { Empty = 0, Man = 1, King = 2, kLightBit = 4 };

constexpr bool isLight(const uint8_t content) { return (content & kLightBit) != 0; }
constexpr bool isKing(const uint8_t content) { return (content & 3) == King; }
constexpr Side sideOf(const uint8_t content) { return isLight(content) ? Light : Dark; }
constexpr bool playable(const int square) { return (((square >> 3) + (square & 7)) & 1) != 0; }

constexpr uint8_t kNone = 0xFF;

// One step of a move: a slide to the next square or one jump.
struct Step {
  uint8_t from = 0;
  uint8_t to = 0;
  uint8_t captured = kNone;  // square of the piece jumped over
};

struct Position {
  uint8_t board[64] = {};
  uint8_t side = Dark;
  // The piece that is in the middle of a multi-jump and must go on, or kNone.
  uint8_t jumper = kNone;
  // Steps since a capture or a move by an uncrowned piece.
  uint8_t quiet = 0;

  void setStart();
  int count(Side side) const;
  bool operator==(const Position& other) const;
};

constexpr int kMaxSteps = 48;

// The legal steps for the side to move: only jumps when a jump exists.
int generateSteps(const Position& position, Step* out);
void applyStep(Position& position, const Step& step);
// Hundredths of a piece, from the point of view of the side to move.
int evaluate(const Position& position);

constexpr int kWinScore = 30000;

struct SearchOptions {
  int maxDepth = 6;
  uint32_t maxNodes = 0;  // 0 = no limit
  // Steps scoring within this margin of the best are candidates and one is
  // picked at random: variety, and a weaker Easy level.
  int margin = 0;
  uint32_t seed = 1;
};

struct SearchResult {
  Step best;
  bool found = false;
  int score = 0;
  int depth = 0;
  uint32_t nodes = 0;
};

using StopFn = bool (*)(void* context);

SearchResult search(const Position& root, const SearchOptions& options, StopFn stop = nullptr,
                    void* context = nullptr);

// A game with its history, for Undo and for spotting repeated positions.
class Game {
 public:
  enum class State : uint8_t { NotStarted, Playing, DarkWins, LightWins, Draw };

  static constexpr int kHistory = 160;
  static constexpr int kSavedHistory = 48;
  static constexpr int kSnapshotBytes = 19;
  static constexpr size_t kSaveBytes = 8 + kSavedHistory * kSnapshotBytes;

  void start();
  State state() const { return state_; }
  bool started() const { return state_ != State::NotStarted; }
  const Position& position() const { return pos_; }
  uint8_t at(const int square) const { return pos_.board[square]; }
  Side sideToMove() const { return static_cast<Side>(pos_.side); }
  // True when the side to move has to capture.
  bool mustCapture() const;
  // The last step played, for marking it on the board; from == to when none.
  Step lastStep() const { return last_; }

  // Squares the piece on `from` may step to now. Returns their number.
  int targets(int from, uint8_t* out) const;
  // Pieces that are able to move now. Returns their number.
  int movable(uint8_t* out) const;
  bool play(int from, int to);
  bool play(const Step& step) { return play(step.from, step.to); }
  // Takes back steps until `side` is at the start of a move again (the whole
  // of the opponent's reply included). Returns false with nothing to undo.
  bool undoTo(Side side);
  bool canUndo() const { return historyCount_ > 0; }

  size_t save(uint8_t out[kSaveBytes]) const;
  bool load(const uint8_t* data, size_t size);

 private:
  struct Snapshot {
    uint32_t men[2] = {0, 0};
    uint32_t kings[2] = {0, 0};
    uint8_t side = Dark;
    uint8_t jumper = kNone;
    uint8_t quiet = 0;
  };

  static Snapshot pack(const Position& position);
  static void unpack(const Snapshot& snapshot, Position& position);
  void updateState();

  Position pos_;
  State state_ = State::NotStarted;
  Step last_;
  Snapshot history_[kHistory];
  int historyCount_ = 0;
};

}  // namespace checkers
