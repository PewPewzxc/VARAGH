#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "DictionaryWordParts.h"

namespace {

struct Part {
  std::string text;
  size_t sourceOffset;
  DictionaryWordPartSeparator separatorBefore;
};

std::vector<Part> splitWordParts(const char* text, const size_t length) {
  std::vector<Part> parts;
  forEachDictionaryWordPart(text, length, [&parts](const DictionaryWordPart& part) {
    parts.push_back({std::string(part.text, part.length), part.sourceOffset, part.separatorBefore});
  });
  return parts;
}

}  // namespace

TEST(DictionaryWordParts, SplitsAsciiHyphenatedCompounds) {
  const auto parts = splitWordParts("half-loft", 9);

  ASSERT_EQ(parts.size(), 2U);
  EXPECT_EQ(parts[0].text, "half");
  EXPECT_EQ(parts[0].sourceOffset, 0U);
  EXPECT_EQ(parts[1].text, "loft");
  EXPECT_EQ(parts[1].sourceOffset, 5U);
  EXPECT_EQ(parts[1].separatorBefore, DictionaryWordPartSeparator::Hyphen);
  EXPECT_STREQ(dictionaryWordPartSeparatorText(parts[1].separatorBefore), "-");
}

TEST(DictionaryWordParts, PreservesTrailingLayoutHyphenForCrossLineLookup) {
  const auto parts = splitWordParts("exter-", 6);

  ASSERT_EQ(parts.size(), 1U);
  EXPECT_EQ(parts[0].text, "exter-");
  EXPECT_EQ(parts[0].sourceOffset, 0U);
  EXPECT_EQ(parts[0].separatorBefore, DictionaryWordPartSeparator::None);
}

TEST(DictionaryWordParts, PreservesAdjacentAsciiHyphens) {
  const auto compound = splitWordParts("foo--bar", 8);
  const auto trailing = splitWordParts("foo--", 5);

  ASSERT_EQ(compound.size(), 1U);
  EXPECT_EQ(compound[0].text, "foo--bar");
  ASSERT_EQ(trailing.size(), 1U);
  EXPECT_EQ(trailing[0].text, "foo--");
}

TEST(DictionaryWordParts, RetainsExistingUnicodeDashSplitting) {
  constexpr char word[] = "half\xE2\x80\x94loft";  // half—loft
  const auto parts = splitWordParts(word, sizeof(word) - 1);

  ASSERT_EQ(parts.size(), 2U);
  EXPECT_EQ(parts[0].text, "half");
  EXPECT_EQ(parts[0].sourceOffset, 0U);
  EXPECT_EQ(parts[1].text, "loft");
  EXPECT_EQ(parts[1].sourceOffset, 7U);
  EXPECT_EQ(parts[1].separatorBefore, DictionaryWordPartSeparator::EmDash);
  EXPECT_STREQ(dictionaryWordPartSeparatorText(parts[1].separatorBefore), "\xE2\x80\x94");
}

TEST(DictionaryWordParts, PositionsRtlCompoundFragmentsInVisualOrder) {
  constexpr int fullWidth = 23;
  constexpr int firstPartWidth = 10;
  constexpr int logicalSecondPartOffset = 13;  // first part plus the hyphen.

  EXPECT_EQ(dictionaryWordPartVisualOffset(fullWidth, 0, firstPartWidth, true), 13);
  EXPECT_EQ(dictionaryWordPartVisualOffset(fullWidth, logicalSecondPartOffset, 10, true), 0);
  EXPECT_EQ(dictionaryWordPartVisualOffset(fullWidth, logicalSecondPartOffset, 10, false), logicalSecondPartOffset);
}

// VARAGH 1.3.1/1.3.2 bug: the word-select buffer was sized as if the hyphen were always dropped, so a
// page where a book hyphen ended a line ("internet-" / "capable") overflowed by one byte: "Memory error".
TEST(DictionaryWordParts, MergedWordBudgetCoversBookHyphen) {
  EXPECT_EQ(mergedLookupLength(6, 4, /*keepHyphen=*/false), 9U);   // exter- + nity -> externity
  EXPECT_EQ(mergedLookupLength(9, 7, /*keepHyphen=*/true), 16U);   // internet- + capable -> internet-capable
  for (size_t first = 1; first < 40; ++first) {
    for (size_t second = 0; second < 40; ++second) {
      const size_t budget = mergedLookupLength(first, second, /*keepHyphen=*/true);
      EXPECT_GE(budget, mergedLookupLength(first, second, /*keepHyphen=*/false));
      EXPECT_GE(budget, first + second);
    }
  }
}
