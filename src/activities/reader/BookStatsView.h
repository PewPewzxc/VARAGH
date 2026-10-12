#pragma once

#include <string>

#include "BookReadingStats.h"
#include "GlobalReadingStats.h"

class GfxRenderer;
class MappedInputManager;

namespace BookStatsTouchTarget {
constexpr int StartedDaysStat = 1;
constexpr int DateFieldBase = 10;
constexpr int DateFieldCount = 6;
constexpr int DateAdjustUp = 20;
constexpr int DateAdjustDown = 21;
constexpr int DateSave = 22;
constexpr int DateCancel = 23;

constexpr int dateField(const int index) { return DateFieldBase + index; }
}  // namespace BookStatsTouchTarget

// titleSuffix (VARAGH): a short text at the right end of the title strip of the
// book's card, the stars of its rating.
void renderPerBookStatsPage(GfxRenderer& renderer, const MappedInputManager* mappedInput, const std::string& bookTitle,
                            const BookReadingStats& stats, float progressPercent, bool hasEstimatedTimeLeft,
                            uint32_t estimatedTimeLeftSeconds, bool showButtonHints, bool showEditButton,
                            bool showMoreButton, const char* titleSuffix = nullptr);
// Where that title strip is on the page with button hints, for a tap on it.
void perBookTitleStripRect(const GfxRenderer& renderer, const MappedInputManager* mappedInput, int& x, int& y,
                           int& width, int& height);

void renderGlobalStatsPage(GfxRenderer& renderer, const MappedInputManager* mappedInput, const char* screenTitle,
                           const GlobalReadingStats& stats, bool showButtonHints, bool showMoreButton);

void renderNoRtcCombinedStatsPage(GfxRenderer& renderer, const MappedInputManager* mappedInput,
                                  const std::string& bookTitle, const BookReadingStats& bookStats,
                                  float progressPercent, bool hasEstimatedTimeLeft, uint32_t estimatedTimeLeftSeconds,
                                  const GlobalReadingStats& deviceStats, const GlobalReadingStats* allDevicesStats,
                                  bool showButtonHints);

void renderEditBookDatesPage(GfxRenderer& renderer, const MappedInputManager* mappedInput, const std::string& bookTitle,
                             const BookReadingStats& stats, int selectedField, bool showButtonHints);
