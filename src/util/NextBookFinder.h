#pragma once

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace NextBookFinder {

// Collects up to maxCount book files that order after currentBookPath's filename
// (natural sort, same ordering as the file browser) within the same folder.
// Returns bare filenames in sorted order; the current file itself is excluded.
// Single directory pass keeping only the maxCount best matches, so memory stays
// bounded regardless of folder size.
std::vector<std::string> findNextBooks(const std::string& currentBookPath, size_t maxCount);

// VARAGH: the book that follows this one in its series (calibre:series and
// series_index, as the Library reads them), in any folder of the card. Uses the
// Library's index; false when the book has no series, is the last one, or the
// index is missing. `position` is the next book's number in the series ("3").
struct SeriesNext {
  std::string path;
  std::string title;
  std::string series;
  std::string position;
};
bool findNextInSeries(const std::string& currentBookPath, SeriesNext& out);

// "3" or "2.5" for a series position stored in the Library's index (the float's
// bits with the sign flipped so they sort as unsigned numbers); empty for an
// unknown one (all bits set).
inline std::string seriesPositionText(const uint32_t storedPosition) {
  if (storedPosition == 0xFFFFFFFFu) return {};
  const uint32_t bits = (storedPosition & 0x80000000u) ? storedPosition ^ 0x80000000u : ~storedPosition;
  float value = 0;
  memcpy(&value, &bits, sizeof(value));
  char text[16];
  snprintf(text, sizeof(text), "%g", static_cast<double>(value));
  return text;
}

}  // namespace NextBookFinder
