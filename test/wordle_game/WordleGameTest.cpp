#include <gtest/gtest.h>

#include <cstring>
#include <set>
#include <string>

#include "src/games/WordleGame.h"

namespace {

std::string marks(const char* answer, const char* guess) {
  wordle::Mark out[wordle::kWordLength];
  wordle::scoreGuess(answer, guess, out);
  std::string text;
  for (const wordle::Mark mark : out) {
    text += mark == wordle::Mark::Correct ? 'C' : (mark == wordle::Mark::Present ? 'P' : '-');
  }
  return text;
}

uint16_t indexOfAnswer(const char* word) {
  char buffer[wordle::kWordLength + 1];
  for (uint16_t i = 0; i < wordle::answerCount(); ++i) {
    wordle::answerAt(i, buffer);
    if (std::strcmp(buffer, word) == 0) return i;
  }
  ADD_FAILURE() << word << " is not an answer";
  return 0;
}

void type(wordle::Game& game, const char* word) {
  for (const char* c = word; *c; ++c) ASSERT_TRUE(game.addLetter(*c));
}

}  // namespace

TEST(WordleWords, AcceptsDictionaryWordsOnly) {
  EXPECT_TRUE(wordle::isAcceptedWord("crane"));
  EXPECT_TRUE(wordle::isAcceptedWord("tiger"));
  EXPECT_TRUE(wordle::isAcceptedWord("aback"));
  EXPECT_TRUE(wordle::isAcceptedWord("zesty"));
  EXPECT_TRUE(wordle::isAcceptedWord("books"));  // a word form, not a headword
  EXPECT_FALSE(wordle::isAcceptedWord("qqqqq"));
  EXPECT_FALSE(wordle::isAcceptedWord("xzxzx"));
  EXPECT_FALSE(wordle::isAcceptedWord("CRANE"));
  EXPECT_FALSE(wordle::isAcceptedWord("cran"));
  EXPECT_FALSE(wordle::isAcceptedWord("cranes"));
  EXPECT_FALSE(wordle::isAcceptedWord(nullptr));
}

TEST(WordleWords, EveryAnswerIsADistinctAcceptedWord) {
  ASSERT_GT(wordle::answerCount(), 1500);
  std::set<std::string> seen;
  char word[wordle::kWordLength + 1];
  for (uint16_t i = 0; i < wordle::answerCount(); ++i) {
    wordle::answerAt(i, word);
    ASSERT_EQ(std::strlen(word), 5u);
    EXPECT_TRUE(wordle::isAcceptedWord(word)) << word;
    EXPECT_TRUE(seen.insert(word).second) << word;
  }
}

TEST(WordleScore, MarksExactAndMisplacedLetters) {
  EXPECT_EQ(marks("tiger", "tiger"), "CCCCC");
  EXPECT_EQ(marks("tiger", "crane"), "-P--P");
  EXPECT_EQ(marks("tiger", "rivet"), "PC-CP");
  EXPECT_EQ(marks("tiger", "bossy"), "-----");
}

TEST(WordleScore, RepeatedLettersAreNotOvercounted) {
  // One 'l' in the answer: only the exact one counts.
  EXPECT_EQ(marks("world", "hello"), "---CP");
  EXPECT_EQ(marks("abbey", "babes"), "PPCC-");
  // The answer holds one 'e'; the second guessed 'e' is the exact match.
  EXPECT_EQ(marks("crane", "eerie"), "--P-C");
  EXPECT_EQ(marks("apple", "poppy"), "P-C--");
}

TEST(WordleGame, PlaysARoundAndTracksTheKeyboard) {
  wordle::Game game;
  EXPECT_FALSE(game.started());
  game.start(indexOfAnswer("tiger"));
  ASSERT_TRUE(game.started());
  EXPECT_STREQ(game.answer(), "tiger");

  type(game, "cran");
  EXPECT_EQ(game.submit(), wordle::Game::Submit::TooShort);
  ASSERT_TRUE(game.addLetter('E'));
  EXPECT_FALSE(game.addLetter('s'));
  EXPECT_EQ(game.submit(), wordle::Game::Submit::Accepted);
  EXPECT_EQ(game.guessCount(), 1);
  EXPECT_EQ(game.typedLength(), 0);
  EXPECT_EQ(game.letterMark('c'), wordle::Mark::Absent);
  EXPECT_EQ(game.letterMark('r'), wordle::Mark::Present);
  EXPECT_EQ(game.letterMark('z'), wordle::Mark::None);

  type(game, "qqqqq");
  EXPECT_EQ(game.submit(), wordle::Game::Submit::NotAWord);
  EXPECT_EQ(game.typedLength(), 5);
  for (int i = 0; i < 5; ++i) EXPECT_TRUE(game.removeLetter());
  EXPECT_FALSE(game.removeLetter());

  type(game, "rivet");
  EXPECT_EQ(game.submit(), wordle::Game::Submit::Accepted);
  EXPECT_EQ(game.letterMark('i'), wordle::Mark::Correct);
  EXPECT_EQ(game.letterMark('r'), wordle::Mark::Present);

  type(game, "tiger");
  EXPECT_EQ(game.submit(), wordle::Game::Submit::Won);
  EXPECT_EQ(game.state(), wordle::Game::State::Won);
  EXPECT_FALSE(game.addLetter('a'));
  EXPECT_EQ(game.stats().played, 1);
  EXPECT_EQ(game.stats().won, 1);
  EXPECT_EQ(game.stats().streak, 1);
  EXPECT_EQ(game.stats().solvedIn[2], 1);
}

TEST(WordleGame, LettersCanGoIntoAnyBox) {
  wordle::Game game;
  EXPECT_FALSE(game.setCursor(2));  // no round yet
  game.start(indexOfAnswer("tiger"));
  EXPECT_EQ(game.cursor(), 0);
  EXPECT_FALSE(game.setCursor(5));
  EXPECT_FALSE(game.removeLetter());

  // Start with the third box and leave the first two for later.
  ASSERT_TRUE(game.setCursor(2));
  ASSERT_TRUE(game.addLetter('g'));
  EXPECT_EQ(game.typedAt(2), 'g');
  EXPECT_EQ(game.typedAt(0), '\0');
  EXPECT_EQ(game.cursor(), 3);
  type(game, "er");
  EXPECT_EQ(game.cursor(), 0);  // nothing empty to the right: on to the first empty box
  EXPECT_EQ(game.typedLength(), 3);
  EXPECT_EQ(game.submit(), wordle::Game::Submit::TooShort);

  ASSERT_TRUE(game.addLetter('l'));
  EXPECT_EQ(game.cursor(), 1);
  ASSERT_TRUE(game.addLetter('i'));
  EXPECT_EQ(game.cursor(), -1);  // full row: no marker
  EXPECT_STREQ(game.typed(), "liger");
  EXPECT_FALSE(game.addLetter('x'));

  // A marked box that holds a letter is replaced by the next letter.
  ASSERT_TRUE(game.setCursor(0));
  ASSERT_TRUE(game.addLetter('t'));
  EXPECT_STREQ(game.typed(), "tiger");
  EXPECT_EQ(game.cursor(), -1);

  // Backspace: the last letter of a full row, then the marked box, then leftwards.
  ASSERT_TRUE(game.removeLetter());
  EXPECT_EQ(game.typedAt(4), '\0');
  EXPECT_EQ(game.cursor(), 4);
  ASSERT_TRUE(game.setCursor(1));
  ASSERT_TRUE(game.removeLetter());
  EXPECT_EQ(game.typedAt(1), '\0');
  EXPECT_EQ(game.cursor(), 1);
  ASSERT_TRUE(game.removeLetter());  // box 1 is empty now: the letter to its left goes
  EXPECT_EQ(game.typedAt(0), '\0');
  EXPECT_EQ(game.cursor(), 0);
  EXPECT_FALSE(game.removeLetter());  // nothing to the left of the first box
  EXPECT_EQ(game.typedLength(), 2);

  // A row with gaps survives a save.
  uint8_t bytes[wordle::Game::kSaveBytes];
  game.save(bytes);
  wordle::Game restored;
  ASSERT_TRUE(restored.load(bytes, sizeof(bytes)));
  EXPECT_EQ(restored.typedAt(0), '\0');
  EXPECT_EQ(restored.typedAt(2), 'g');
  EXPECT_EQ(restored.typedAt(3), 'e');
  EXPECT_EQ(restored.cursor(), 0);
  type(restored, "ti");
  EXPECT_EQ(restored.cursor(), 4);
  ASSERT_TRUE(restored.addLetter('r'));
  EXPECT_EQ(restored.submit(), wordle::Game::Submit::Won);
  EXPECT_EQ(restored.cursor(), -1);  // round over
}

TEST(WordleGame, SixWrongGuessesLoseAndResetTheStreak) {
  wordle::Game game;
  game.start(indexOfAnswer("tiger"));
  type(game, "tiger");
  ASSERT_EQ(game.submit(), wordle::Game::Submit::Won);

  game.start(indexOfAnswer("crane"));
  for (int i = 0; i < 5; ++i) {
    type(game, "bossy");
    ASSERT_EQ(game.submit(), wordle::Game::Submit::Accepted);
  }
  type(game, "bossy");
  EXPECT_EQ(game.submit(), wordle::Game::Submit::Lost);
  EXPECT_EQ(game.state(), wordle::Game::State::Lost);
  EXPECT_EQ(game.stats().played, 2);
  EXPECT_EQ(game.stats().won, 1);
  EXPECT_EQ(game.stats().streak, 0);
  EXPECT_EQ(game.stats().bestStreak, 1);

  game.start(indexOfAnswer("crane"));
  type(game, "bossy");
  ASSERT_EQ(game.submit(), wordle::Game::Submit::Accepted);
  game.giveUp();
  EXPECT_EQ(game.state(), wordle::Game::State::Lost);
  EXPECT_EQ(game.stats().played, 3);
}

TEST(WordleGame, SaveAndLoadRestoreTheRound) {
  wordle::Game game;
  game.start(indexOfAnswer("tiger"));
  type(game, "crane");
  ASSERT_EQ(game.submit(), wordle::Game::Submit::Accepted);
  type(game, "ti");

  uint8_t bytes[wordle::Game::kSaveBytes];
  ASSERT_EQ(game.save(bytes), sizeof(bytes));

  wordle::Game restored;
  ASSERT_TRUE(restored.load(bytes, sizeof(bytes)));
  EXPECT_STREQ(restored.answer(), "tiger");
  EXPECT_EQ(restored.guessCount(), 1);
  EXPECT_STREQ(restored.guess(0), "crane");
  EXPECT_EQ(restored.mark(0, 1), wordle::Mark::Present);
  EXPECT_STREQ(restored.typed(), "ti");
  EXPECT_EQ(restored.letterMark('c'), wordle::Mark::Absent);
  type(restored, "ger");
  EXPECT_EQ(restored.submit(), wordle::Game::Submit::Won);

  bytes[0] ^= 0xFF;
  wordle::Game untouched;
  EXPECT_FALSE(untouched.load(bytes, sizeof(bytes)));
  EXPECT_FALSE(untouched.started());
  EXPECT_FALSE(untouched.load(bytes, 10));
}

TEST(WordleGame, StatisticsSurviveAnUnstartedSave) {
  wordle::Game game;
  game.start(indexOfAnswer("tiger"));
  type(game, "tiger");
  ASSERT_EQ(game.submit(), wordle::Game::Submit::Won);

  uint8_t bytes[wordle::Game::kSaveBytes];
  game.save(bytes);
  wordle::Game restored;
  ASSERT_TRUE(restored.load(bytes, sizeof(bytes)));
  EXPECT_EQ(restored.state(), wordle::Game::State::Won);
  EXPECT_EQ(restored.stats().won, 1);
  EXPECT_EQ(restored.stats().solvedIn[0], 1);
}
