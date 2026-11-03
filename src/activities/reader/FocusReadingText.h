#pragma once

#include <EpdFontFamily.h>
#include <Epub/PersianFocus.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace FocusReadingText {

// Calls drawRun for the two visual runs that make up a Focus Reading word.
// drawRun must consume text synchronously because the bold prefix uses this
// small stack buffer, matching TextBlock::render().
template <typename DrawRun>
bool drawSplitRuns(const char* word, const size_t wordLength, const uint8_t focusBoundary, const int wordX,
                   const uint16_t secondRunOffset, const EpdFontFamily::Style style, const bool isRtl,
                   DrawRun&& drawRun) {
  if (focusBoundary == 0) {
    return false;
  }

  const auto boldStyle = static_cast<EpdFontFamily::Style>(style | EpdFontFamily::BOLD);
  char boldText[40];
  char restText[96];
  // Same runs as TextBlock::render(): a joined Persian cut gets a ZWJ on both sides.
  const bool split = PersianFocus::splitRuns(word, wordLength, focusBoundary, boldText, sizeof(boldText), restText,
                                             sizeof(restText));
  const size_t boldLength = std::min<size_t>({static_cast<size_t>(focusBoundary), wordLength, sizeof(boldText) - 1});
  if (!split) {
    memcpy(boldText, word, boldLength);
    boldText[boldLength] = '\0';
  }
  const char* const rest = split ? restText : word + boldLength;
  const int secondRunX = wordX + secondRunOffset;
  if (isRtl) {
    drawRun(wordX, rest, style, false);
    // Thickened like TextBlock::render(), so Persian bold reads as bold.
    drawRun(secondRunX, boldText, boldStyle, PersianFocus::startsArabic(boldText));
  } else {
    drawRun(wordX, boldText, boldStyle, false);
    drawRun(secondRunX, rest, style, false);
  }
  return true;
}

}  // namespace FocusReadingText
