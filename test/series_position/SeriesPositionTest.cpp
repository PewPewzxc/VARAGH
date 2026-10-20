#include "NextBookFinder.h"

#include <gtest/gtest.h>

#include <cstring>

namespace {
// The Library builder's encoding (LibraryBuilder.cpp, parseSeriesPosition).
uint32_t stored(const float value) {
  uint32_t bits = 0;
  const float normalized = value == 0 ? 0 : value;
  memcpy(&bits, &normalized, sizeof(bits));
  return bits & 0x80000000u ? ~bits : bits ^ 0x80000000u;
}
}  // namespace

TEST(SeriesPosition, ShowsTheNumberTheBookCarries) {
  EXPECT_EQ(NextBookFinder::seriesPositionText(stored(3)), "3");
  EXPECT_EQ(NextBookFinder::seriesPositionText(stored(1)), "1");
  EXPECT_EQ(NextBookFinder::seriesPositionText(stored(2.5f)), "2.5");
  EXPECT_EQ(NextBookFinder::seriesPositionText(stored(0)), "0");
  EXPECT_EQ(NextBookFinder::seriesPositionText(stored(-1)), "-1");
  EXPECT_EQ(NextBookFinder::seriesPositionText(0xFFFFFFFFu), "");
}

TEST(SeriesPosition, StoredOrderIsNumberOrder) {
  EXPECT_LT(stored(1), stored(2));
  EXPECT_LT(stored(2), stored(2.5f));
  EXPECT_LT(stored(-1), stored(0));
  EXPECT_LT(stored(9), stored(10));
}
