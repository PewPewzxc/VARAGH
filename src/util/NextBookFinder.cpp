#include "NextBookFinder.h"

#include <FsHelpers.h>
#include <HalStorage.h>
#include <Logging.h>
#include <Memory.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string_view>

#include "CrossPointSettings.h"
#include "LibraryBuilder.h"
#include "LibraryIndexFile.h"

namespace {
constexpr size_t NAME_BUFFER_SIZE = 500;

bool isSupportedBookFile(const std::string_view name) {
  // Formats ReaderActivity can open (bmp is a viewer, not a book, so it is excluded)
  return FsHelpers::hasEpubExtension(name) || FsHelpers::hasXtcExtension(name) || FsHelpers::hasTxtExtension(name) ||
         FsHelpers::hasMarkdownExtension(name);
}
}  // namespace

std::vector<std::string> NextBookFinder::findNextBooks(const std::string& currentBookPath, const size_t maxCount) {
  std::vector<std::string> result;
  if (maxCount == 0 || currentBookPath.empty()) {
    return result;
  }

  const std::string folder = FsHelpers::extractFolderPath(currentBookPath);
  const auto lastSlash = currentBookPath.find_last_of('/');
  const std::string currentName =
      lastSlash == std::string::npos ? currentBookPath : currentBookPath.substr(lastSlash + 1);

  auto dir = Storage.open(folder.c_str());
  if (!dir || !dir.isDirectory()) {
    LOG_ERR("NBF", "Cannot open folder: %s", folder.c_str());
    return result;
  }
  dir.rewindDirectory();

  const auto nameBuffer = makeUniqueNoThrow<char[]>(NAME_BUFFER_SIZE);
  if (!nameBuffer) {
    LOG_ERR("NBF", "OOM: %d bytes", static_cast<int>(NAME_BUFFER_SIZE));
    dir.close();
    return result;
  }

  // Heap use is bounded: at most maxCount+1 short filename strings live at once (the
  // file browser holds a whole folder in the same std::string form). A failed
  // allocation here would abort like any STL growth in this codebase; the reserve
  // below makes vector growth a single up-front allocation.
  result.reserve(maxCount + 1);
  const auto less = [](const std::string& a, const std::string& b) { return FsHelpers::naturalLess(a, b); };

  for (auto file = dir.openNextFile(); file; file = dir.openNextFile()) {
    if (file.isDirectory()) {
      continue;
    }
    file.getName(nameBuffer.get(), NAME_BUFFER_SIZE);
    if (!SETTINGS.showHiddenFiles && nameBuffer[0] == '.') {
      continue;
    }
    if (!isSupportedBookFile(nameBuffer.get())) {
      continue;
    }
    std::string name{nameBuffer.get()};
    // Keep only files ordering strictly after the current one; equal names (the book
    // itself, or a case-variant of it) compare "not less" both ways and drop out here.
    if (!FsHelpers::naturalLess(currentName, name)) {
      continue;
    }
    // Bounded insertion sort: keep the maxCount lowest-ordering candidates
    if (result.size() >= maxCount && !less(name, result.back())) {
      continue;
    }
    const auto pos = std::lower_bound(result.begin(), result.end(), name, less);
    result.insert(pos, std::move(name));
    if (result.size() > maxCount) {
      result.pop_back();
    }
  }
  dir.close();

  return result;
}

bool NextBookFinder::findNextInSeries(const std::string& currentBookPath, SeriesNext& out) {
  if (currentBookPath.empty()) return false;
  library::LibraryIndexFile index;
  if (!index.open(library::libraryIndexPath())) return false;

  const library::BookIdentity identity{library::clixPathHash(currentBookPath.data(), currentBookPath.size()), 0};
  uint16_t recentRow = 0xFFFF;
  if (!index.recentRowsFor(&identity, 1, &recentRow) || recentRow == 0xFFFF) return false;
  const uint16_t current = index.ordinalForRow(library::SortOrder::RecentAsc, recentRow);
  library::ClixRecord record{};
  std::string series;
  uint32_t position = library::CLIX_UNKNOWN_SERIES_POSITION;
  if (current == 0xFFFF || !index.readRecord(current, record) || !index.readSeries(record, series) || series.empty() ||
      !index.readSeriesPosition(record, position) || position == library::CLIX_UNKNOWN_SERIES_POSITION) {
    return false;
  }

  // Series order sorts by series name, then position: the next book is the
  // first one after this one with the same series and a higher number.
  const uint16_t count = index.bookCount();
  uint16_t row = 0;
  while (row < count && index.ordinalForRow(library::SortOrder::SeriesAsc, row) != current) ++row;
  for (++row; row < count; ++row) {
    const uint16_t ordinal = index.ordinalForRow(library::SortOrder::SeriesAsc, row);
    library::ClixRecord candidate{};
    std::string candidateSeries;
    uint32_t candidatePosition = library::CLIX_UNKNOWN_SERIES_POSITION;
    if (ordinal == 0xFFFF || !index.readRecord(ordinal, candidate) || !index.readSeries(candidate, candidateSeries)) {
      return false;
    }
    if (candidateSeries != series) return false;  // past the end of this series
    if (!index.readSeriesPosition(candidate, candidatePosition) ||
        candidatePosition == library::CLIX_UNKNOWN_SERIES_POSITION || candidatePosition <= position) {
      continue;  // same number (another copy) or no number
    }
    std::string author;
    if (!index.readPath(candidate, out.path) || !index.readDisplayText(candidate, out.title, author)) return false;
    if (out.path == currentBookPath) continue;
    out.series = series;
    out.position = seriesPositionText(candidatePosition);
    return true;
  }
  return false;
}
