#include "PersianFocus.h"

#include <gtest/gtest.h>

#include <string>

// The bold part of a Persian word ends only at a natural break, so no joined
// letter is cut from its neighbour; vowel marks stay with their letter.
namespace {

std::string bold(const std::string& word) {
  const size_t cut = PersianFocus::boundary(word, 43);
  return word.substr(0, cut);
}

}  // namespace

TEST(PersianFocus, HalfSpaceIsTheBreak) {
  EXPECT_EQ(bold("می‌روم"), "می‌");
  EXPECT_EQ(bold("نمی‌دانم"), "نمی‌");
}

TEST(PersianFocus, BreaksAfterNonJoiningLetters) {
  EXPECT_EQ(bold("کتاب"), "کتا");      // after alef
  EXPECT_EQ(bold("دانشگاه"), "دا");    // after dal, then alef: closest to the target
  EXPECT_EQ(bold("روزنامه"), "روز");  // three breaks; the third is closest to 43 %
}

TEST(PersianFocus, NoBreakMeansNoBold) {
  EXPECT_EQ(bold("بیش"), "");          // fully joined
  EXPECT_EQ(bold("سبک"), "");
  EXPECT_EQ(bold("و"), "");            // a single letter
}

TEST(PersianFocus, VowelMarksStayWithTheirLetter) {
  // ka-taab with zebar and tashdid: the break after alef keeps the marks before it.
  const std::string word = "کَتّاب";
  const std::string b = bold(word);
  EXPECT_EQ(b, "کَتّا");
  // The pish after dal comes along with the dal.
  EXPECT_EQ(bold("دُرُست"), "دُ");
}

TEST(PersianFocus, NeverBoldsTheWholeWord) {
  EXPECT_EQ(bold("دارو"), "د");  // the final non-joiner is never a break
  EXPECT_EQ(bold("آرزو"), "آ");
}

TEST(PersianFocus, OnlyArabicScriptWords) {
  EXPECT_TRUE(PersianFocus::startsArabic("کتاب"));
  EXPECT_TRUE(PersianFocus::startsArabic("«کتاب»"));
  EXPECT_FALSE(PersianFocus::startsArabic("book"));
  EXPECT_FALSE(PersianFocus::startsArabic("Buch"));
}
