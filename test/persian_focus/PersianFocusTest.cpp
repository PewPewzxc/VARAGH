#include "PersianFocus.h"

#include <gtest/gtest.h>

#include <string>

// Every Persian word gets the first 40 % of its letters in bold (rounded down,
// at least one letter); a cut inside a join keeps the letters joined.
namespace {

std::string bold(const std::string& word) {
  const size_t cut = PersianFocus::boundary(word, PersianFocus::BOLD_PERCENT);
  return word.substr(0, cut);
}

}  // namespace

TEST(PersianFocus, FortyPercentRoundedDown) {
  EXPECT_EQ(bold("دانشگاه"), "دا");      // 7 letters -> 2
  EXPECT_EQ(bold("کتاب‌ها"), "کت");      // 6 letters -> 2
  EXPECT_EQ(bold("روزنامه‌ها"), "روز");  // 9 letters -> 3
  EXPECT_EQ(bold("خواندنشان"), "خوا");   // 9 letters -> 3
  EXPECT_EQ(bold("دوست"), "د");          // 4 letters -> 1
}

TEST(PersianFocus, EveryWordGetsAtLeastOneLetter) {
  EXPECT_EQ(bold("من"), "م");
  EXPECT_EQ(bold("روز"), "ر");
  EXPECT_EQ(bold("و"), "و");  // a one-letter word is bold in full
}

TEST(PersianFocus, HalfSpaceIsNotALetter) {
  EXPECT_EQ(bold("می‌روم"), "می‌");  // 5 letters -> 2, the half-space stays with them
  EXPECT_EQ(bold("نمی‌دانم"), "نم");  // 7 letters -> 2
}

TEST(PersianFocus, VowelMarksAreNotLetters) {
  EXPECT_EQ(bold("کَتّاب"), "کَ");    // 4 letters -> 1; the zebar stays with its letter
  EXPECT_EQ(bold("دُرُست"), "دُ");  // 4 letters -> 1
}

TEST(PersianFocus, JoinedCutKeepsTheLettersJoined) {
  const std::string word = "کتاب";  // 4 letters -> 1: ک joins the ت after it
  char boldRun[40];
  char restRun[96];
  ASSERT_TRUE(PersianFocus::splitRuns(word.c_str(), word.size(), PersianFocus::boundary(word, 40), boldRun,
                                      sizeof(boldRun), restRun, sizeof(restRun)));
  EXPECT_EQ(std::string(boldRun), "ک‍");
  EXPECT_EQ(std::string(restRun), "‍تاب");

  const std::string unjoined = "دانشگاه";  // cut after alef, which never joins forward
  ASSERT_TRUE(PersianFocus::splitRuns(unjoined.c_str(), unjoined.size(), PersianFocus::boundary(unjoined, 40),
                                      boldRun, sizeof(boldRun), restRun, sizeof(restRun)));
  EXPECT_EQ(std::string(boldRun), "دا");
  EXPECT_EQ(std::string(restRun), "نشگاه");
}

TEST(PersianFocus, OnlyArabicScriptWords) {
  EXPECT_TRUE(PersianFocus::startsArabic("کتاب"));
  EXPECT_TRUE(PersianFocus::startsArabic("«کتاب»"));
  EXPECT_FALSE(PersianFocus::startsArabic("book"));
  EXPECT_FALSE(PersianFocus::startsArabic("Buch"));
}
