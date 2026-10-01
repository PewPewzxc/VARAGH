#include <gtest/gtest.h>

#include "ReaderStatusBarConfig.h"
#include "ReaderStatusBarJson.h"

TEST(ReaderStatusBarConfig, MigratesSixLegacyItemsIntoSevenSlots) {
  const LegacyReaderStatusBarSettings old{true, true, true, 1, 2, true, 2, 0, 2};
  const auto bar = migrateBottomStatusBar(old);
  EXPECT_EQ(bar.slots[0], ReaderStatusBarItem::Battery);
  EXPECT_EQ(bar.slots[1], ReaderStatusBarItem::TimeLeftBook);
  EXPECT_EQ(bar.slots[2], ReaderStatusBarItem::Empty);
  EXPECT_EQ(bar.slots[3], ReaderStatusBarItem::TitleChapter);
  EXPECT_EQ(bar.slots[4], ReaderStatusBarItem::ChapterPageCount);
  EXPECT_EQ(bar.slots[5], ReaderStatusBarItem::StablePageNumber);
  EXPECT_EQ(bar.slots[6], ReaderStatusBarItem::BookProgressPercentage);
  EXPECT_EQ(bar.percentageFormat, 2);
  EXPECT_EQ(bar.progressBar, 0);
  EXPECT_EQ(bar.progressBarThickness, 2);
}

TEST(ReaderStatusBarConfig, LegacyXtcTopInitiallyUsesMigratedBottomItems) {
  const auto bottom = migrateBottomStatusBar({true, true, true, 1, 2, true, 2, 0, 2});
  ReaderStatusBarConfig top;
  top.slots[ReaderStatusBarConfig::CENTER] = ReaderStatusBarItem::Clock;
  const auto select = [&](const ReaderStatusBarPosition displayed,
                          const bool legacyTopUsesBottom) -> const ReaderStatusBarConfig& {
    return xtcStatusBarConfigPosition(displayed, legacyTopUsesBottom) == ReaderStatusBarPosition::Top ? top : bottom;
  };
  EXPECT_EQ(select(ReaderStatusBarPosition::Top, true).slots, bottom.slots);
  EXPECT_EQ(select(ReaderStatusBarPosition::Top, false).slots, top.slots);
  EXPECT_EQ(select(ReaderStatusBarPosition::Bottom, true).slots, bottom.slots);
}

TEST(ReaderStatusBarConfig, EmptySlotsDoNotReserveSpaceAndDuplicatesStayIndependent) {
  ReaderStatusBarConfig bar;
  bar.slots = {ReaderStatusBarItem::Battery,   ReaderStatusBarItem::Empty, ReaderStatusBarItem::Empty,
               ReaderStatusBarItem::TitleBook, ReaderStatusBarItem::Empty, ReaderStatusBarItem::Battery,
               ReaderStatusBarItem::Empty};
  EXPECT_TRUE(bar.contains(ReaderStatusBarItem::Battery));
  const std::array<int, ReaderStatusBarConfig::SLOT_COUNT> widths{16, 0, 0, 0, 0, 16, 0};
  const auto placement = layoutReaderStatusBarItems(10, 190, widths);
  EXPECT_EQ(placement.x[0], 10);
  EXPECT_EQ(placement.x[5], 174);
  EXPECT_EQ(placement.centerLeft, 34);
  EXPECT_EQ(placement.centerRight, 166);
}

TEST(ReaderStatusBarConfig, LoneSideTitleUsesFreeRowWidth) {
  std::array<int, ReaderStatusBarConfig::SLOT_COUNT> widths{500, 0, 0, 0, 0, 0, 0};
  fitReaderStatusBarSideWidths(widths, 700);
  EXPECT_EQ(widths[0], 500);
  const auto placement = layoutReaderStatusBarItems(10, 710, widths);
  EXPECT_EQ(placement.x[0], 10);
  EXPECT_EQ(placement.centerLeft, 518);
  EXPECT_EQ(placement.centerRight, 710);
}

TEST(ReaderStatusBarConfig, CrowdedSidesFitWithoutOverlap) {
  std::array<int, ReaderStatusBarConfig::SLOT_COUNT> widths{200, 24, 24, 0, 200, 24, 24};
  fitReaderStatusBarSideWidths(widths, 400, 13);
  const auto placement = layoutReaderStatusBarItems(10, 410, widths, 13);
  EXPECT_GT(placement.x[2], placement.x[1]);
  EXPECT_LE(placement.centerLeft, placement.centerRight);
  EXPECT_EQ(placement.x[6] + widths[6], 410);
  EXPECT_EQ(widths[1], 24);
  EXPECT_EQ(widths[2], 24);
  EXPECT_EQ(widths[5], 24);
}

TEST(ReaderStatusBarConfig, ProgressOnlyBottomReservesBookmarkAndAutoTurnText) {
  EXPECT_EQ(readerStatusBarTotalHeight(ReaderStatusBarPosition::Top, false, 7, 19), 7);
  EXPECT_EQ(readerStatusBarTotalHeight(ReaderStatusBarPosition::Bottom, false, 7, 19), 14);
  EXPECT_EQ(readerStatusBarTotalHeight(ReaderStatusBarPosition::Bottom, true, 7, 19), 26);
}

TEST(ReaderStatusBarConfig, BookmarkStaysAtBottomLeftAndRightItemsAnchorToEdge) {
  const std::array<int, ReaderStatusBarConfig::SLOT_COUNT> widths{20, 0, 0, 0, 15, 0, 10};
  const auto placement = layoutReaderStatusBarItems(10, 190, widths, 13);
  EXPECT_EQ(placement.x[0], 31);
  EXPECT_EQ(placement.x[4], 157);
  EXPECT_EQ(placement.x[6], 180);
  EXPECT_LE(placement.centerLeft, placement.centerRight);
}

TEST(ReaderStatusBarConfig, RejectsInvalidSavedSlotsAndUnavailableClock) {
  EXPECT_TRUE(validReaderStatusBarItemValue(static_cast<int>(ReaderStatusBarItem::Clock), true));
  EXPECT_FALSE(validReaderStatusBarItemValue(static_cast<int>(ReaderStatusBarItem::Clock), false));
  EXPECT_FALSE(validReaderStatusBarItemValue(-1, true));
  EXPECT_FALSE(validReaderStatusBarItemValue(static_cast<int>(ReaderStatusBarItem::Count), true));
  EXPECT_TRUE(validReaderStatusBarChoice(2, 3));
  EXPECT_FALSE(validReaderStatusBarChoice(3, 3));
}

TEST(ReaderStatusBarConfig, WebPayloadRoundTripsBothBarsWithDuplicates) {
  ReaderStatusBarConfig top;
  top.slots = {ReaderStatusBarItem::Clock,
               ReaderStatusBarItem::Battery,
               ReaderStatusBarItem::TimeLeftBook,
               ReaderStatusBarItem::TitleBook,
               ReaderStatusBarItem::Battery,
               ReaderStatusBarItem::Empty,
               ReaderStatusBarItem::BookProgressPercentage};
  top.percentageFormat = 2;
  top.progressBar = 0;
  JsonDocument outgoing;
  writeReaderStatusBarJson(outgoing["top"].to<JsonObject>(), top);
  writeReaderStatusBarJson(outgoing["bottom"].to<JsonObject>(), migrateBottomStatusBar({}));
  outgoing["xtcMode"] = 3;

  std::string saved;
  serializeJson(outgoing, saved);
  JsonDocument reloaded;
  ASSERT_FALSE(deserializeJson(reloaded, saved));
  ReaderStatusBarsPayload loaded;
  ASSERT_TRUE(readReaderStatusBarsPayload(reloaded.as<JsonVariantConst>(), loaded, true, 3, 3, 3, 4));
  EXPECT_EQ(loaded.top.slots, top.slots);
  EXPECT_EQ(loaded.top.percentageFormat, 2);
  EXPECT_EQ(loaded.top.progressBar, 0);
  EXPECT_EQ(loaded.xtcMode, 3);
}

TEST(ReaderStatusBarConfig, WebPayloadRequiresBothBarsAndValidXtcModeBeforeSaving) {
  JsonDocument request;
  writeReaderStatusBarJson(request["top"].to<JsonObject>(), ReaderStatusBarConfig{});
  request["xtcMode"] = 2;
  ReaderStatusBarsPayload parsed;
  EXPECT_FALSE(readReaderStatusBarsPayload(request.as<JsonVariantConst>(), parsed, true, 3, 3, 3, 4));
  writeReaderStatusBarJson(request["bottom"].to<JsonObject>(), ReaderStatusBarConfig{});
  EXPECT_TRUE(readReaderStatusBarsPayload(request.as<JsonVariantConst>(), parsed, true, 3, 3, 3, 4));
  request["xtcMode"] = 4;
  EXPECT_FALSE(readReaderStatusBarsPayload(request.as<JsonVariantConst>(), parsed, true, 3, 3, 3, 4));
  EXPECT_EQ(parsed.xtcMode, 2);
}

TEST(ReaderStatusBarConfig, WebPayloadRejectsInvalidOrIncompleteBars) {
  JsonDocument payload;
  auto bar = payload["top"].to<JsonObject>();
  writeReaderStatusBarJson(bar, ReaderStatusBarConfig{});
  ReaderStatusBarConfig loaded;
  EXPECT_TRUE(readReaderStatusBarJson(payload["top"], loaded, true, 3, 3, 3));
  bar["slots"][0] = 99;
  EXPECT_FALSE(readReaderStatusBarJson(payload["top"], loaded, true, 3, 3, 3));
  bar["slots"][0] = 1;
  EXPECT_FALSE(readReaderStatusBarJson(payload["top"], loaded, false, 3, 3, 3));
  bar["slots"][0] = 0;
  bar["percentageFormat"] = 3;
  EXPECT_FALSE(readReaderStatusBarJson(payload["top"], loaded, true, 3, 3, 3));
  bar["percentageFormat"] = 0;
  bar["slots"].as<JsonArray>().remove(6);
  EXPECT_FALSE(readReaderStatusBarJson(payload["top"], loaded, true, 3, 3, 3));
}

TEST(ReaderStatusBarConfig, RepairsDamagedSavedValuesWithoutDroppingValidSlots) {
  JsonDocument saved;
  auto bar = saved["bottom"].to<JsonObject>();
  ReaderStatusBarConfig original;
  original.slots = {ReaderStatusBarItem::Battery,
                    ReaderStatusBarItem::Clock,
                    ReaderStatusBarItem::Empty,
                    ReaderStatusBarItem::TitleBook,
                    ReaderStatusBarItem::ChapterPageCount,
                    ReaderStatusBarItem::Empty,
                    ReaderStatusBarItem::Empty};
  writeReaderStatusBarJson(bar, original);
  bar["slots"][4] = 99;
  bar["thickness"] = 99;

  ReaderStatusBarConfig repaired;
  EXPECT_TRUE(repairReaderStatusBarJson(saved["bottom"], repaired, false, 3, 3, 3));
  EXPECT_EQ(repaired.slots[0], ReaderStatusBarItem::Battery);
  EXPECT_EQ(repaired.slots[1], ReaderStatusBarItem::Empty);
  EXPECT_EQ(repaired.slots[3], ReaderStatusBarItem::TitleBook);
  EXPECT_EQ(repaired.slots[4], ReaderStatusBarItem::Empty);
  EXPECT_EQ(repaired.progressBarThickness, 1);
}

TEST(ReaderStatusBarConfig, MigratesSavedSixSlotBarsAndResavesSevenSlots) {
  JsonDocument saved;
  JsonArray topSlots = saved["top"]["slots"].to<JsonArray>();
  for (const auto item : {ReaderStatusBarItem::Clock, ReaderStatusBarItem::Battery, ReaderStatusBarItem::TitleBook,
                          ReaderStatusBarItem::StablePageNumber, ReaderStatusBarItem::TimeLeftChapter,
                          ReaderStatusBarItem::BookProgressPercentage}) {
    topSlots.add(static_cast<int>(item));
  }
  saved["top"]["percentageFormat"] = 2;
  saved["top"]["progressBar"] = 0;
  saved["top"]["thickness"] = 2;

  JsonArray bottomSlots = saved["bottom"]["slots"].to<JsonArray>();
  for (const auto item : {ReaderStatusBarItem::Battery, ReaderStatusBarItem::TimeLeftBook,
                          ReaderStatusBarItem::TitleChapter, ReaderStatusBarItem::ChapterPageCount,
                          ReaderStatusBarItem::StablePageNumber, ReaderStatusBarItem::BookProgressPercentage}) {
    bottomSlots.add(static_cast<int>(item));
  }
  saved["bottom"]["percentageFormat"] = 1;
  saved["bottom"]["progressBar"] = 1;
  saved["bottom"]["thickness"] = 0;

  ReaderStatusBarConfig top;
  ReaderStatusBarConfig bottom;
  EXPECT_TRUE(repairReaderStatusBarJson(saved["top"], top, true, 3, 3, 3));
  EXPECT_TRUE(repairReaderStatusBarJson(saved["bottom"], bottom, true, 3, 3, 3));
  const std::array expectedTop{ReaderStatusBarItem::Clock,
                               ReaderStatusBarItem::Battery,
                               ReaderStatusBarItem::Empty,
                               ReaderStatusBarItem::TitleBook,
                               ReaderStatusBarItem::StablePageNumber,
                               ReaderStatusBarItem::TimeLeftChapter,
                               ReaderStatusBarItem::BookProgressPercentage};
  const std::array expectedBottom{ReaderStatusBarItem::Battery,
                                  ReaderStatusBarItem::TimeLeftBook,
                                  ReaderStatusBarItem::Empty,
                                  ReaderStatusBarItem::TitleChapter,
                                  ReaderStatusBarItem::ChapterPageCount,
                                  ReaderStatusBarItem::StablePageNumber,
                                  ReaderStatusBarItem::BookProgressPercentage};
  EXPECT_EQ(top.slots, expectedTop);
  EXPECT_EQ(bottom.slots, expectedBottom);
  EXPECT_EQ(top.percentageFormat, 2);
  EXPECT_EQ(top.progressBar, 0);
  EXPECT_EQ(top.progressBarThickness, 2);
  EXPECT_EQ(bottom.percentageFormat, 1);
  EXPECT_EQ(bottom.progressBar, 1);
  EXPECT_EQ(bottom.progressBarThickness, 0);

  JsonDocument resaved;
  writeReaderStatusBarJson(resaved["top"].to<JsonObject>(), top);
  writeReaderStatusBarJson(resaved["bottom"].to<JsonObject>(), bottom);
  EXPECT_EQ(resaved["top"]["slots"].as<JsonArrayConst>().size(), ReaderStatusBarConfig::SLOT_COUNT);
  EXPECT_EQ(resaved["bottom"]["slots"].as<JsonArrayConst>().size(), ReaderStatusBarConfig::SLOT_COUNT);
  EXPECT_FALSE(repairReaderStatusBarJson(resaved["top"], top, true, 3, 3, 3));
  EXPECT_FALSE(repairReaderStatusBarJson(resaved["bottom"], bottom, true, 3, 3, 3));
}

TEST(ReaderStatusBarConfig, MigratesValidSixSlotItemsWhenAnotherItemIsInvalid) {
  JsonDocument saved;
  JsonArray slots = saved["slots"].to<JsonArray>();
  for (const int item : {2, 1, 9, 5, 99, 7}) slots.add(item);
  saved["progressBar"] = 0;

  ReaderStatusBarConfig config;
  EXPECT_TRUE(repairReaderStatusBarJson(saved.as<JsonVariantConst>(), config, false, 3, 3, 3));
  EXPECT_EQ(config.slots[0], ReaderStatusBarItem::Battery);
  EXPECT_EQ(config.slots[1], ReaderStatusBarItem::Empty);
  EXPECT_EQ(config.slots[2], ReaderStatusBarItem::Empty);
  EXPECT_EQ(config.slots[3], ReaderStatusBarItem::TitleChapter);
  EXPECT_EQ(config.slots[4], ReaderStatusBarItem::ChapterPageCount);
  EXPECT_EQ(config.slots[5], ReaderStatusBarItem::Empty);
  EXPECT_EQ(config.slots[6], ReaderStatusBarItem::BookProgressPercentage);
  EXPECT_EQ(config.progressBar, 0);
}

TEST(ReaderStatusBarConfig, UnknownLengthSavedBarKeepsCurrentDefault) {
  JsonDocument saved;
  JsonArray slots = saved["slots"].to<JsonArray>();
  for (int i = 0; i < 5; ++i) slots.add(static_cast<int>(ReaderStatusBarItem::Battery));
  ReaderStatusBarConfig config;
  config.slots[ReaderStatusBarConfig::CENTER] = ReaderStatusBarItem::TitleChapter;

  EXPECT_TRUE(repairReaderStatusBarJson(saved.as<JsonVariantConst>(), config, true, 3, 3, 3));
  EXPECT_EQ(config.slots[ReaderStatusBarConfig::LEFT_THIRD], ReaderStatusBarItem::Empty);
  EXPECT_EQ(config.slots[ReaderStatusBarConfig::CENTER], ReaderStatusBarItem::TitleChapter);
}

TEST(ReaderStatusBarLayout, TopTextUsesHomeInsetAndOnlyEnabledProgressReservesSpace) {
  constexpr int textLane = 19;
  EXPECT_EQ(readerStatusBarTotalHeight(ReaderStatusBarPosition::Top, true, 0, textLane), 21);
  EXPECT_EQ(readerStatusBarTotalHeight(ReaderStatusBarPosition::Top, true, 7, textLane), 28);
  EXPECT_EQ(readerStatusBarTotalHeight(ReaderStatusBarPosition::Top, false, 0, textLane), 0);
  EXPECT_EQ(readerStatusBarTotalHeight(ReaderStatusBarPosition::Top, false, 7, textLane), 7);
  // Bottom text and progress-only bookmark reservations retain their existing bounds.
  EXPECT_EQ(readerStatusBarTotalHeight(ReaderStatusBarPosition::Bottom, true, 0, textLane), 19);
  EXPECT_EQ(readerStatusBarTotalHeight(ReaderStatusBarPosition::Bottom, false, 7, textLane), 14);
}
