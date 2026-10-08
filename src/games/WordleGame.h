#pragma once

#include <cstddef>
#include <cstdint>

// Rules and state of the built-in Wordle game. No drawing, no storage and no
// heap: the activity owns one Game and persists it through save()/load().
namespace wordle {

constexpr int kWordLength = 5;
constexpr int kMaxGuesses = 6;

enum class Mark : uint8_t {
  None = 0,     // letter not judged yet
  Absent = 1,   // not in the word
  Present = 2,  // in the word, other position
  Correct = 3,  // in the word, this position
};

// The word data (see scripts/build_wordle_words.py).
bool isAcceptedWord(const char* word);
uint16_t answerCount();
// Writes the answer as five lowercase letters plus a terminator.
void answerAt(uint16_t index, char out[kWordLength + 1]);

// Marks a guess against the answer; both are five lowercase letters. A letter
// is Present at most as often as the answer still holds it after the exact
// matches, as in the original game.
void scoreGuess(const char* answer, const char* guess, Mark out[kWordLength]);

struct Stats {
  uint16_t played = 0;
  uint16_t won = 0;
  uint16_t streak = 0;
  uint16_t bestStreak = 0;
  uint16_t solvedIn[kMaxGuesses] = {};  // games won with 1..6 guesses
};

class Game {
 public:
  enum class State : uint8_t { Playing = 0, Won = 1, Lost = 2 };
  enum class Submit : uint8_t { TooShort, NotAWord, Accepted, Won, Lost };

  // Starts a round with the answer at answerIndex (taken modulo answerCount()).
  void start(uint16_t answerIndex);

  // The row being typed is five boxes and a marker. A letter goes into the
  // marked box, so the boxes can be filled in any order.
  //
  // Moves the marker to a box (0..4) of the row being typed.
  bool setCursor(int column);
  // The marked box, or -1 when the row is full or the round is over.
  int cursor() const { return state_ == State::Playing && cursor_ < kWordLength ? cursor_ : -1; }
  // Writes a letter (a-z or A-Z) into the marked box, replacing what it held.
  // The marker then goes to the next empty box to the right, wrapping round
  // to the start; a full row has no marker and takes no more letters.
  bool addLetter(char letter);
  // Clears the marked box when it holds a letter; otherwise the nearest letter
  // to its left (the last letter when the row is full), and marks that box.
  bool removeLetter();
  // Judges the typed row. Won/Lost also update the statistics.
  Submit submit();
  // Ends a running round as lost, so the answer can be shown.
  void giveUp();

  State state() const { return state_; }
  bool started() const { return answer_[0] != '\0'; }
  const char* answer() const { return answer_; }
  uint16_t answerIndex() const { return answerIndex_; }
  int guessCount() const { return guessCount_; }
  const char* guess(int row) const { return guesses_[row]; }
  Mark mark(int row, int column) const { return marks_[row][column]; }
  // The row being typed as text. It ends at the first empty box, so with gaps
  // read the boxes one by one with typedAt().
  const char* typed() const { return typed_; }
  // Letter in a box of the row being typed, 0 when the box is empty.
  char typedAt(int column) const { return typed_[column]; }
  // How many boxes of the row being typed hold a letter.
  int typedLength() const;
  // Best mark each letter has received so far, for the keyboard.
  Mark letterMark(char letter) const;
  const Stats& stats() const { return stats_; }

  // Fixed-size snapshot of the round and the statistics.
  static constexpr size_t kSaveBytes = 96;
  size_t save(uint8_t out[kSaveBytes]) const;
  // False (state untouched) when the bytes are not a snapshot of this format.
  bool load(const uint8_t* data, size_t size);

 private:
  char answer_[kWordLength + 1] = {};
  uint16_t answerIndex_ = 0;
  char guesses_[kMaxGuesses][kWordLength + 1] = {};
  Mark marks_[kMaxGuesses][kWordLength] = {};
  uint8_t guessCount_ = 0;
  char typed_[kWordLength + 1] = {};  // 0 = empty box
  uint8_t cursor_ = 0;                // kWordLength = no marker
  State state_ = State::Playing;
  Mark letterMarks_[26] = {};
  Stats stats_;

  void rebuildLetterMarks();
};

}  // namespace wordle
