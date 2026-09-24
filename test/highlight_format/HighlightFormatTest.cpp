#include <gtest/gtest.h>

#include "HighlightFormat.h"

using namespace HighlightFormat;

TEST(HighlightFormat, EntryRoundTripsSpecialCharacters) {
  Entry e;
  e.text = "Häuser\tmit Tab";
  e.meaning = "house <n>\nline two \\ backslash";
  e.context = "Die Häuser sind alt.";
  e.source = "Der Prozess";
  e.addedAt = 1790000000u;
  const std::string line = entryLine(e);
  EXPECT_EQ(line.find('\n'), std::string::npos);
  Entry back;
  ASSERT_TRUE(parseEntry(line, back));
  EXPECT_EQ(back.text, e.text);
  EXPECT_EQ(back.meaning, e.meaning);
  EXPECT_EQ(back.context, e.context);
  EXPECT_EQ(back.source, e.source);
  EXPECT_EQ(back.addedAt, e.addedAt);
}

TEST(HighlightFormat, HeaderRoundTripAndPersian) {
  const std::string name = "\xD9\x88\xD8\xA7\xDA\x98\xD9\x87\xE2\x80\x8C\xD9\x87\xD8\xA7";  // Persian with ZWNJ
  CategoryType type = CategoryType::Notes;
  std::string parsed;
  ASSERT_TRUE(parseHeader(headerLine(CategoryType::Flashcards, name), type, parsed));
  EXPECT_EQ(type, CategoryType::Flashcards);
  EXPECT_EQ(parsed, name);
  EXPECT_FALSE(parseHeader("garbage", type, parsed));
}

TEST(HighlightFormat, OldOrShortLinesStillParse) {
  Entry e;
  ASSERT_TRUE(parseEntry("just text", e));
  EXPECT_EQ(e.text, "just text");
  EXPECT_TRUE(e.meaning.empty());
  EXPECT_FALSE(parseEntry("", e));
}

TEST(HighlightFormat, LeitnerLevels) {
  EXPECT_EQ(levelAfterAnswer(0, true), 2);
  EXPECT_EQ(levelAfterAnswer(3, true), 4);
  EXPECT_EQ(levelAfterAnswer(5, true), 5);
  EXPECT_EQ(levelAfterAnswer(4, false), 1);
  EXPECT_EQ(levelAfterAnswer(0, false), 1);
  const std::vector<uint8_t> levels{3, 0, 1, 5, 1};
  const std::vector<uint16_t> expected{1, 2, 4, 0, 3};
  EXPECT_EQ(reviewOrder(levels), expected);
}

TEST(HighlightFormat, DefinitionHtmlToText) {
  const std::string html =
      "1. <b>Haus</b> <i>/hˈaʊs/ &lt;neut, n, sg&gt;</i><br>&nbsp;&nbsp;house &lt;n&gt;<br><br>2. building";
  EXPECT_EQ(plainTextFromDefinition(html, 500), "1. Haus /hˈaʊs/ <neut, n, sg>\nhouse <n>\n2. building");
}

TEST(HighlightFormat, DefinitionCutKeepsWholeCharacters) {
  const std::string text = "über";  // ü is two bytes
  EXPECT_EQ(plainTextFromDefinition(text, 1), "");
  EXPECT_EQ(plainTextFromDefinition(text, 2), "\xC3\xBC");
  EXPECT_EQ(plainTextFromDefinition(text, 3), "\xC3\xBC" "b");
}
