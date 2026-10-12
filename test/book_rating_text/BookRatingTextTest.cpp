#include <gtest/gtest.h>

#include <string>

#include "src/util/BookRatingText.h"

using bookrating::ListLine;

TEST(BookRatingText, WritesTheNumber) {
  EXPECT_EQ(bookrating::number(0), "");
  EXPECT_EQ(bookrating::number(1), "0.5");
  EXPECT_EQ(bookrating::number(7), "3.5");
  EXPECT_EQ(bookrating::number(8), "4");
  EXPECT_EQ(bookrating::number(10), "5");
}

TEST(BookRatingText, DrawsFiveStarSymbols) {
  const std::string full = "\xE2\x98\x85";
  const std::string half = "\xE2\xAF\xAA";
  const std::string empty = "\xE2\x98\x86";
  EXPECT_EQ(bookrating::stars(0), empty + empty + empty + empty + empty);
  EXPECT_EQ(bookrating::stars(1), half + empty + empty + empty + empty);
  EXPECT_EQ(bookrating::stars(7), full + full + full + half + empty);
  EXPECT_EQ(bookrating::stars(8), full + full + full + full + empty);
  EXPECT_EQ(bookrating::stars(10), full + full + full + full + full);
}

TEST(BookRatingText, ATapSetsHalfOrWholeAndTheSameAgainClears) {
  // Third star: its left half is 2.5, its right half 3.
  EXPECT_EQ(bookrating::afterTap(0, 2, false), 5);
  EXPECT_EQ(bookrating::afterTap(0, 2, true), 6);
  EXPECT_EQ(bookrating::afterTap(8, 0, false), 1);
  EXPECT_EQ(bookrating::afterTap(3, 4, true), 10);
  // The value that is already set goes away.
  EXPECT_EQ(bookrating::afterTap(6, 2, true), 0);
  EXPECT_EQ(bookrating::afterTap(5, 2, false), 0);
}

TEST(BookRatingText, ReadsNumbersAndRoundsToHalfStars) {
  EXPECT_EQ(bookrating::parseNumber("3.5"), 7);
  EXPECT_EQ(bookrating::parseNumber("3,5"), 7);
  EXPECT_EQ(bookrating::parseNumber(" 4"), 8);
  EXPECT_EQ(bookrating::parseNumber("5"), 10);
  EXPECT_EQ(bookrating::parseNumber("0.5"), 1);
  EXPECT_EQ(bookrating::parseNumber("3.7"), 7);
  EXPECT_EQ(bookrating::parseNumber("3.9"), 8);
  EXPECT_EQ(bookrating::parseNumber("9"), 10);
  EXPECT_EQ(bookrating::parseNumber(""), 0);
  EXPECT_EQ(bookrating::parseNumber("good"), 0);
  EXPECT_EQ(bookrating::parseNumber("0"), 0);
}

TEST(BookRatingText, AListLineGoesThereAndBack) {
  const std::string line = bookrating::formatLine(7, "Pride and Prejudice", "Jane Austen", "/books/pride.epub");
  EXPECT_EQ(line, "3.5\tPride and Prejudice\tJane Austen\t/books/pride.epub");
  // ListLine points into the text it was read from, so that text has to stay.
  const std::string fromFile = line + "\r\n";
  ListLine entry;
  ASSERT_TRUE(bookrating::parseLine(fromFile, entry));
  EXPECT_EQ(entry.halfStars, 7);
  EXPECT_EQ(entry.title, "Pride and Prejudice");
  EXPECT_EQ(entry.author, "Jane Austen");
  EXPECT_EQ(entry.path, "/books/pride.epub");

  // A book without an author, and a path with spaces.
  ASSERT_TRUE(bookrating::parseLine("5\tFaust\t\t/Read/Faust (2).epub", entry));
  EXPECT_EQ(entry.halfStars, 10);
  EXPECT_EQ(entry.author, "");
  EXPECT_EQ(entry.path, "/Read/Faust (2).epub");
}

TEST(BookRatingText, NamesCannotBreakALineApart) {
  const std::string line = bookrating::formatLine(4, "Two\tWords", "A\nB", "/x.epub");
  EXPECT_EQ(line, "2\tTwo Words\tA B\t/x.epub");
  ListLine entry;
  ASSERT_TRUE(bookrating::parseLine(line, entry));
  EXPECT_EQ(entry.title, "Two Words");
}

TEST(BookRatingText, SkipsLinesThatAreNotRatings) {
  ListLine entry;
  EXPECT_FALSE(bookrating::parseLine("", entry));
  EXPECT_FALSE(bookrating::parseLine("# VARAGH book ratings", entry));
  EXPECT_FALSE(bookrating::parseLine("3.5\tOnly a title", entry));
  EXPECT_FALSE(bookrating::parseLine("\tTitle\tAuthor\t/x.epub", entry));
  EXPECT_FALSE(bookrating::parseLine("4\tTitle\tAuthor\t", entry));
}
