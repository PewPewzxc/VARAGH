#include <gtest/gtest.h>

#include <vector>

#include "util/WordSelectNavigator.h"

using Words = std::vector<WordSelectNavigator::WordInfo>;

namespace {
int step(const Words& words, const int start, const int end, const int delta) {
  return WordSelectNavigator::resizedSelectionEnd(words, start, end, delta);
}
}  // namespace

TEST(WordSelectStep, PlainWordsMoveOne) {
  const Words words(4);
  EXPECT_EQ(step(words, 0, 0, 1), 1);
  EXPECT_EQ(step(words, 0, 2, -1), 1);
}

TEST(WordSelectStep, GluedPiecesMoveTogether) {
  // "one Hel|lo two": "lo" sits right after "Hel" with no space between.
  Words words(4);
  words[2].joinWithoutSpaceBefore = 1;
  EXPECT_EQ(step(words, 0, 0, 1), 2);  // one press adds the whole "Hello"
  EXPECT_EQ(step(words, 0, 2, -1), 0);  // and one press takes it off again
  EXPECT_EQ(step(words, 0, 2, 1), 3);
}

TEST(WordSelectStep, LineBreakHalvesMoveTogether) {
  // "one exter- / nity two": the second half continues the first.
  Words words(4);
  words[2].continuationOf = 1;
  EXPECT_EQ(step(words, 0, 0, 1), 2);
  EXPECT_EQ(step(words, 0, 2, -1), 0);
}

TEST(WordSelectStep, StaysInsideThePage) {
  Words words(3);
  words[1].joinWithoutSpaceBefore = 1;
  EXPECT_EQ(step(words, 0, 0, -1), -1);  // never below one word
  EXPECT_EQ(step(words, 0, 1, -1), -1);  // the start word is one "Hello"
  EXPECT_EQ(step(words, 0, 2, 1), -1);   // nothing after the last word
}
