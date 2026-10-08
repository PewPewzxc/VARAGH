#include "WordleGame.h"

#include <algorithm>
#include <cstring>

#include "WordleWords.h"

namespace wordle {

namespace {

constexpr uint8_t kSaveMagic[4] = {'V', 'W', 'D', '1'};

bool isLowerLetters(const char* word) {
  for (int i = 0; i < kWordLength; ++i) {
    if (word[i] < 'a' || word[i] > 'z') return false;
  }
  return word[kWordLength] == '\0';
}

void putU16(uint8_t*& out, const uint16_t value) {
  *out++ = static_cast<uint8_t>(value & 0xFF);
  *out++ = static_cast<uint8_t>(value >> 8);
}

uint16_t getU16(const uint8_t*& in) {
  const uint16_t value = static_cast<uint16_t>(in[0] | (in[1] << 8));
  in += 2;
  return value;
}

}  // namespace

bool isAcceptedWord(const char* word) {
  if (!word || !isLowerLetters(word)) return false;
  const int bucket = (word[0] - 'a') * 26 + (word[1] - 'a');
  const uint16_t tail = static_cast<uint16_t>(((word[2] - 'a') * 26 + (word[3] - 'a')) * 26 + (word[4] - 'a'));
  const uint16_t* first = wordle_words::kTail + wordle_words::kBucketStart[bucket];
  const uint16_t* last = wordle_words::kTail + wordle_words::kBucketStart[bucket + 1];
  return std::binary_search(first, last, tail);
}

uint16_t answerCount() { return wordle_words::kAnswerCount; }

void answerAt(const uint16_t index, char out[kWordLength + 1]) {
  const uint16_t wordIndex = wordle_words::kAnswers[index % wordle_words::kAnswerCount];
  // The bucket is the last one starting at or before the word.
  const uint16_t* end = wordle_words::kBucketStart + 677;
  const int bucket = static_cast<int>(std::upper_bound(wordle_words::kBucketStart, end, wordIndex) -
                                      wordle_words::kBucketStart) - 1;
  uint16_t tail = wordle_words::kTail[wordIndex];
  out[0] = static_cast<char>('a' + bucket / 26);
  out[1] = static_cast<char>('a' + bucket % 26);
  out[4] = static_cast<char>('a' + tail % 26);
  tail /= 26;
  out[3] = static_cast<char>('a' + tail % 26);
  out[2] = static_cast<char>('a' + tail / 26);
  out[5] = '\0';
}

void scoreGuess(const char* answer, const char* guess, Mark out[kWordLength]) {
  uint8_t unmatched[26] = {};
  for (int i = 0; i < kWordLength; ++i) {
    if (guess[i] == answer[i]) {
      out[i] = Mark::Correct;
    } else {
      out[i] = Mark::Absent;
      ++unmatched[answer[i] - 'a'];
    }
  }
  for (int i = 0; i < kWordLength; ++i) {
    if (out[i] == Mark::Correct) continue;
    uint8_t& left = unmatched[guess[i] - 'a'];
    if (left > 0) {
      out[i] = Mark::Present;
      --left;
    }
  }
}

void Game::start(const uint16_t answerIndex) {
  answerIndex_ = static_cast<uint16_t>(answerIndex % answerCount());
  answerAt(answerIndex_, answer_);
  std::memset(guesses_, 0, sizeof(guesses_));
  std::memset(marks_, 0, sizeof(marks_));
  std::memset(typed_, 0, sizeof(typed_));
  std::memset(letterMarks_, 0, sizeof(letterMarks_));
  guessCount_ = 0;
  cursor_ = 0;
  state_ = State::Playing;
}

int Game::typedLength() const {
  int count = 0;
  for (int column = 0; column < kWordLength; ++column) count += typed_[column] != '\0' ? 1 : 0;
  return count;
}

bool Game::setCursor(const int column) {
  if (state_ != State::Playing || !started() || column < 0 || column >= kWordLength) return false;
  cursor_ = static_cast<uint8_t>(column);
  return true;
}

bool Game::addLetter(char letter) {
  if (letter >= 'A' && letter <= 'Z') letter = static_cast<char>(letter - 'A' + 'a');
  if (letter < 'a' || letter > 'z') return false;
  if (state_ != State::Playing || !started() || cursor_ >= kWordLength) return false;
  typed_[cursor_] = letter;
  uint8_t next = kWordLength;
  for (int step = 1; step < kWordLength; ++step) {
    const int column = (cursor_ + step) % kWordLength;
    if (typed_[column] == '\0') {
      next = static_cast<uint8_t>(column);
      break;
    }
  }
  cursor_ = next;
  return true;
}

bool Game::removeLetter() {
  if (state_ != State::Playing || !started()) return false;
  if (cursor_ < kWordLength && typed_[cursor_] != '\0') {
    typed_[cursor_] = '\0';
    return true;
  }
  for (int column = static_cast<int>(cursor_) - 1; column >= 0; --column) {
    if (typed_[column] == '\0') continue;
    typed_[column] = '\0';
    cursor_ = static_cast<uint8_t>(column);
    return true;
  }
  return false;
}

Game::Submit Game::submit() {
  if (state_ != State::Playing || !started() || typedLength() < kWordLength) return Submit::TooShort;
  if (!isAcceptedWord(typed_)) return Submit::NotAWord;

  std::memcpy(guesses_[guessCount_], typed_, kWordLength + 1);
  scoreGuess(answer_, typed_, marks_[guessCount_]);
  ++guessCount_;
  std::memset(typed_, 0, sizeof(typed_));
  cursor_ = 0;
  rebuildLetterMarks();

  const bool solved = std::memcmp(guesses_[guessCount_ - 1], answer_, kWordLength) == 0;
  if (!solved && guessCount_ < kMaxGuesses) return Submit::Accepted;

  if (stats_.played < UINT16_MAX) ++stats_.played;
  if (solved) {
    state_ = State::Won;
    if (stats_.won < UINT16_MAX) ++stats_.won;
    if (stats_.streak < UINT16_MAX) ++stats_.streak;
    stats_.bestStreak = std::max(stats_.bestStreak, stats_.streak);
    uint16_t& bucket = stats_.solvedIn[guessCount_ - 1];
    if (bucket < UINT16_MAX) ++bucket;
    return Submit::Won;
  }
  state_ = State::Lost;
  stats_.streak = 0;
  return Submit::Lost;
}

void Game::giveUp() {
  if (state_ != State::Playing || !started()) return;
  std::memset(typed_, 0, sizeof(typed_));
  cursor_ = 0;
  state_ = State::Lost;
  if (stats_.played < UINT16_MAX) ++stats_.played;
  stats_.streak = 0;
}

Mark Game::letterMark(char letter) const {
  if (letter >= 'A' && letter <= 'Z') letter = static_cast<char>(letter - 'A' + 'a');
  if (letter < 'a' || letter > 'z') return Mark::None;
  return letterMarks_[letter - 'a'];
}

void Game::rebuildLetterMarks() {
  std::memset(letterMarks_, 0, sizeof(letterMarks_));
  for (int row = 0; row < guessCount_; ++row) {
    for (int column = 0; column < kWordLength; ++column) {
      Mark& best = letterMarks_[guesses_[row][column] - 'a'];
      best = std::max(best, marks_[row][column]);
    }
  }
}

size_t Game::save(uint8_t out[kSaveBytes]) const {
  std::memset(out, 0, kSaveBytes);
  uint8_t* cursor = out;
  std::memcpy(cursor, kSaveMagic, sizeof(kSaveMagic));
  cursor += sizeof(kSaveMagic);
  putU16(cursor, answerIndex_);
  *cursor++ = started() ? 1 : 0;
  *cursor++ = static_cast<uint8_t>(state_);
  *cursor++ = guessCount_;
  // Rows saved before the boxes could be filled in any order hold a count of
  // letters here, which is the same number: the first empty box.
  *cursor++ = cursor_;
  std::memcpy(cursor, typed_, kWordLength);
  cursor += kWordLength;
  for (int row = 0; row < kMaxGuesses; ++row) {
    std::memcpy(cursor, guesses_[row], kWordLength);
    cursor += kWordLength;
  }
  putU16(cursor, stats_.played);
  putU16(cursor, stats_.won);
  putU16(cursor, stats_.streak);
  putU16(cursor, stats_.bestStreak);
  for (const uint16_t count : stats_.solvedIn) putU16(cursor, count);
  return kSaveBytes;
}

bool Game::load(const uint8_t* data, const size_t size) {
  if (!data || size < kSaveBytes || std::memcmp(data, kSaveMagic, sizeof(kSaveMagic)) != 0) return false;
  const uint8_t* cursor = data + sizeof(kSaveMagic);
  const uint16_t answerIndex = getU16(cursor);
  const bool wasStarted = *cursor++ != 0;
  const uint8_t state = *cursor++;
  const uint8_t guessCount = *cursor++;
  const uint8_t marker = *cursor++;
  if (answerIndex >= answerCount() || state > static_cast<uint8_t>(State::Lost) || guessCount > kMaxGuesses ||
      marker > kWordLength) {
    return false;
  }

  Game loaded;
  if (wasStarted) loaded.start(answerIndex);
  for (int i = 0; i < kWordLength; ++i) {
    if (cursor[i] != 0 && (cursor[i] < 'a' || cursor[i] > 'z')) return false;
    if (wasStarted) loaded.typed_[i] = static_cast<char>(cursor[i]);
  }
  loaded.cursor_ = wasStarted ? marker : 0;
  cursor += kWordLength;
  for (int row = 0; row < kMaxGuesses; ++row) {
    if (wasStarted && row < guessCount) {
      std::memcpy(loaded.guesses_[row], cursor, kWordLength);
      if (!isLowerLetters(loaded.guesses_[row])) return false;
      scoreGuess(loaded.answer_, loaded.guesses_[row], loaded.marks_[row]);
    }
    cursor += kWordLength;
  }
  loaded.guessCount_ = wasStarted ? guessCount : 0;
  loaded.state_ = wasStarted ? static_cast<State>(state) : State::Playing;
  loaded.rebuildLetterMarks();
  loaded.stats_.played = getU16(cursor);
  loaded.stats_.won = getU16(cursor);
  loaded.stats_.streak = getU16(cursor);
  loaded.stats_.bestStreak = getU16(cursor);
  for (uint16_t& count : loaded.stats_.solvedIn) count = getU16(cursor);

  *this = loaded;
  return true;
}

}  // namespace wordle
