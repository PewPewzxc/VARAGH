#include <gtest/gtest.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <numeric>
#include <string>
#include <vector>

#include "Epub.h"
#include "LibraryBuilder.h"
#include "LibraryFileTypes.h"
#include "LibraryIndexFile.h"
#include "LibraryText.h"

using namespace library;

std::vector<std::string> preservedCacheClears;
bool preserveCacheState = true;

bool clearBookCachePreservingUserState(const std::string& path) {
  preservedCacheClears.push_back(path);
  return preserveCacheState;
}

namespace {

constexpr char INDEX[] = "/.crosspoint/library.idx";

std::string numbered(const char* prefix, const unsigned value) {
  char text[32];
  std::snprintf(text, sizeof(text), "%s%04u", prefix, value);
  return text;
}

std::string pathAt(LibraryIndexFile& index, const SortOrder order, const uint16_t row) {
  const uint16_t ordinal = index.ordinalForRow(order, row);
  if (ordinal == 0xFFFF) return {};
  ClixRecord record{};
  if (!index.readRecord(ordinal, record)) return {};
  std::string path;
  return index.readPath(record, path) ? path : std::string();
}

bool recordAtPath(LibraryIndexFile& index, const std::string& path, ClixRecord& out) {
  for (uint16_t ordinal = 0; ordinal < index.bookCount(); ordinal++) {
    ClixRecord record{};
    std::string storedPath;
    if (!index.readRecord(ordinal, record) || !index.readPath(record, storedPath)) return false;
    if (storedPath == path) {
      out = record;
      return true;
    }
  }
  return false;
}

// Build a genuine V5 name section by removing each V6 position and updating
// record offsets. Changing only the version byte would leave V6 blobs behind.
bool downgradeIndexToVersionFive() {
  auto& bytes = fake::files[INDEX]->bytes;
  ClixHeader header{};
  std::memcpy(&header, bytes.data(), sizeof(header));
  std::vector<uint8_t> old(bytes.begin(), bytes.begin() + header.nameStart);
  for (uint16_t i = 0; i < header.bookCount; i++) {
    ClixRecord record{};
    std::memcpy(&record, bytes.data() + recordOffset(header, i), sizeof(record));
    uint32_t next = header.nameLen;
    if (i + 1 < header.bookCount) {
      ClixRecord following{};
      std::memcpy(&following, bytes.data() + recordOffset(header, i + 1), sizeof(following));
      next = following.nameOff;
    }
    if (next < record.nameOff + sizeof(uint32_t) || next > header.nameLen) return false;
    const uint32_t newOffset = static_cast<uint32_t>(old.size() - header.nameStart);
    old.insert(old.end(), bytes.begin() + header.nameStart + record.nameOff,
               bytes.begin() + header.nameStart + next - sizeof(uint32_t));
    record.nameOff = newOffset;
    std::memcpy(old.data() + recordOffset(header, i), &record, sizeof(record));
  }
  header.formatVersion = 5;
  header.nameLen = static_cast<uint32_t>(old.size() - header.nameStart);
  header.selfSize = static_cast<uint32_t>(old.size());
  std::memcpy(old.data(), &header, sizeof(header));
  bytes = std::move(old);
  return true;
}

class LibraryBuilderTest : public ::testing::Test {
 protected:
  BuildStats stats;

  void SetUp() override {
    fake::reset();
    invalidateLibraryIndex();
    bookMetadata.clear();
    cachedBookMetadata.clear();
    metadataCacheUse.clear();
    preservedCacheClears.clear();
    preserveCacheState = true;
    fake::add("/a.epub");
    fake::add("/b.epub");
  }

  void initial() { ASSERT_TRUE(buildLibraryIndex("/", stats, true)); }
};

}  // namespace

TEST_F(LibraryBuilderTest, UnchangedRebuildReusesMetadataAndDoesNotReplaceIndex) {
  initial();
  const auto old = fake::files[INDEX]->bytes;
  fake::parses = 0;

  ASSERT_TRUE(buildLibraryIndex("/", stats, true));

  EXPECT_EQ(fake::parses, 0u);
  EXPECT_EQ(stats.parsed, 0);
  EXPECT_EQ(stats.metadataReused, 2);
  EXPECT_FALSE(stats.indexReplaced);
  EXPECT_EQ(fake::files[INDEX]->bytes, old);
}

TEST_F(LibraryBuilderTest, MissingModificationDateClearsDerivedCacheThroughStatePreservingPath) {
  fake::files["/a.epub"]->time = 0;
  initial();
  preservedCacheClears.clear();
  metadataCacheUse.clear();

  ASSERT_TRUE(buildLibraryIndex("/", stats, true));

  EXPECT_EQ(preservedCacheClears, std::vector<std::string>{"/a.epub"});
  ASSERT_EQ(metadataCacheUse.size(), 1u);
  EXPECT_FALSE(metadataCacheUse.front());
}

TEST_F(LibraryBuilderTest, ChangedEpubAbortsIndexRefreshIfReadingStateCannotBePreserved) {
  initial();
  fake::files["/a.epub"]->time++;
  preserveCacheState = false;
  fake::parses = 0;

  EXPECT_FALSE(buildLibraryIndex("/", stats, true));
  EXPECT_EQ(preservedCacheClears, std::vector<std::string>{"/a.epub"});
  EXPECT_EQ(fake::parses, 0u);
  EXPECT_TRUE(Storage.exists(INDEX));
}

TEST_F(LibraryBuilderTest, FolderHeavyUnchangedReconciliationIoScalesLinearly) {
  const auto measure = [this](const unsigned count) {
    fake::reset();
    bookMetadata.clear();
    for (unsigned i = 0; i < count; i++) {
      fake::add("/folder" + numbered("", i) + "/book.txt");
    }
    if (!buildLibraryIndex("/", stats, false)) {
      ADD_FAILURE() << "initial build failed for " << count << " books";
      return 0u;
    }
    fake::resetIoCounters();
    if (!buildLibraryIndex("/", stats, false)) {
      ADD_FAILURE() << "unchanged build failed for " << count << " books";
      return 0u;
    }
    EXPECT_EQ(stats.metadataReused, count);
    EXPECT_FALSE(stats.indexReplaced);
    return fake::reads + fake::seeks;
  };

  const unsigned smallIo = measure(128);
  const unsigned largeIo = measure(256);
  EXPECT_LT(largeIo, smallIo * 3u);
}

TEST_F(LibraryBuilderTest, DirectoryEntriesAreEnumeratedOnce) {
  fake::add("/folder/c.txt");

  ASSERT_TRUE(buildLibraryIndex("/", stats, false));

  EXPECT_EQ(fake::directoryEntriesByPath["/a.epub"], 1u);
  EXPECT_EQ(fake::directoryEntriesByPath["/b.epub"], 1u);
  EXPECT_EQ(fake::directoryEntriesByPath["/folder"], 1u);
  EXPECT_EQ(fake::directoryEntriesByPath["/folder/c.txt"], 1u);
}

TEST_F(LibraryBuilderTest, FileTypesKeepMarkdownSeparateAndIncludeXtch) {
  EXPECT_EQ(fileTypeFor("book.epub"), FileEpub);
  EXPECT_EQ(fileTypeFor("book.xtc"), FileXtc);
  EXPECT_EQ(fileTypeFor("book.xtch"), FileXtc);
  EXPECT_EQ(fileTypeFor("book.txt"), FileTxt);
  EXPECT_EQ(fileTypeFor("book.md"), FileMarkdown);
  fake::add("/graphic.xtch");
  ASSERT_TRUE(buildLibraryIndex("/", stats, false));
  LibraryIndexFile index;
  ASSERT_TRUE(index.open(INDEX));
  ClixRecord record{};
  EXPECT_TRUE(recordAtPath(index, "/graphic.xtch", record));
}

TEST_F(LibraryBuilderTest, DirectoryResumeFailureRetainsPreviousIndex) {
  initial();
  const auto old = fake::files[INDEX]->bytes;
  fake::add("/aa-folder/c.txt");
  fake::failDirectorySeek = true;

  EXPECT_FALSE(buildLibraryIndex("/", stats, false));
  EXPECT_EQ(fake::files[INDEX]->bytes, old);
}

TEST_F(LibraryBuilderTest, DirectoryIterationFailureRetainsPreviousIndex) {
  // openNextFile() returning falsy is ambiguous between "reached the end of
  // the directory" and an SdFat allocation/iteration error partway through.
  // A card glitch mid-listing must abort the build, not silently look like a
  // fully (if short) scanned folder.
  initial();
  const auto old = fake::files[INDEX]->bytes;
  fake::failDirectoryIterationPath = "/";

  EXPECT_FALSE(buildLibraryIndex("/", stats, false));
  EXPECT_TRUE(fake::failureTriggered);
  EXPECT_EQ(fake::files[INDEX]->bytes, old);
}

TEST_F(LibraryBuilderTest, DirectoryOpenFailureRetainsPreviousIndex) {
  initial();
  const auto old = fake::files[INDEX]->bytes;
  fake::failOpenPath = "/";

  EXPECT_FALSE(buildLibraryIndex("/", stats, false));
  EXPECT_TRUE(fake::failureTriggered);
  EXPECT_EQ(fake::files[INDEX]->bytes, old);
}

TEST_F(LibraryBuilderTest, StagingAndIndexWritesAreBatched) {
  fake::reset();
  for (unsigned i = 0; i < 128; i++) fake::add("/book" + numbered("", i) + ".txt");

  ASSERT_TRUE(buildLibraryIndex("/", stats, false));

  EXPECT_LT(fake::writesByPath["/.crosspoint/library.stage"], 64u);
  EXPECT_LT(fake::writesByPath["/.crosspoint/library.new"], 32u);
}

TEST_F(LibraryBuilderTest, ParentDuplicateTrackingSurvivesDirectoryRecursion) {
  fake::add("/folder/c.txt");
  fake::duplicateDirectoryEntry("/a.epub");

  ASSERT_TRUE(buildLibraryIndex("/", stats, false));

  EXPECT_EQ(stats.books, 3);
  EXPECT_EQ(stats.duplicatesDropped, 1);
}

TEST_F(LibraryBuilderTest, TimestampAndSizeChangesParseOnlyTheChangedBook) {
  initial();
  fake::files["/a.epub"]->time++;
  fake::parses = 0;
  ASSERT_TRUE(buildLibraryIndex("/", stats, true));
  EXPECT_EQ(fake::parses, 1u);
  EXPECT_EQ(stats.metadataReused, 1);

  fake::files["/b.epub"]->bytes.push_back('x');
  fake::parses = 0;
  ASSERT_TRUE(buildLibraryIndex("/", stats, true));
  EXPECT_EQ(fake::parses, 1u);
  EXPECT_EQ(stats.metadataReused, 1);
}

TEST_F(LibraryBuilderTest, MetadataCacheIsBypassedOnlyForChangedSources) {
  ASSERT_TRUE(buildLibraryIndex("/", stats, true));
  ASSERT_EQ(metadataCacheUse.size(), 2u);
  EXPECT_TRUE(metadataCacheUse[0]);
  EXPECT_TRUE(metadataCacheUse[1]);

  metadataCacheUse.clear();
  fake::files["/a.epub"]->time++;
  ASSERT_TRUE(buildLibraryIndex("/", stats, true));
  ASSERT_EQ(metadataCacheUse.size(), 1u);
  EXPECT_FALSE(metadataCacheUse[0]);

  metadataCacheUse.clear();
  fake::files["/b.epub"]->bytes.push_back('x');
  ASSERT_TRUE(buildLibraryIndex("/", stats, true));
  ASSERT_EQ(metadataCacheUse.size(), 1u);
  EXPECT_FALSE(metadataCacheUse[0]);

  metadataCacheUse.clear();
  ASSERT_TRUE(buildLibraryIndex("/", stats, true));
  EXPECT_TRUE(metadataCacheUse.empty());
}

TEST_F(LibraryBuilderTest, MetadataStagingTruncatesAtUtf8Boundaries) {
  const std::string title(254, 'T');
  const std::string author(127, 'A');
  const std::string emojiTitle(252, 'E');
  const std::string emojiAuthor(125, 'B');
  bookMetadata["/a.epub"].title = title + "\xC3\xA9";
  bookMetadata["/a.epub"].author = author + "\xC3\xA9";
  bookMetadata["/b.epub"].title = emojiTitle + "\xF0\x9F\x98\x80";
  bookMetadata["/b.epub"].author = emojiAuthor + "\xF0\x9F\x98\x80";

  ASSERT_TRUE(buildLibraryIndex("/", stats, true));
  LibraryIndexFile index;
  ASSERT_TRUE(index.open(INDEX));
  ClixRecord aRecord{};
  ASSERT_TRUE(recordAtPath(index, "/a.epub", aRecord));
  std::string storedTitle;
  std::string storedAuthor;
  ASSERT_TRUE(index.readTitle(aRecord, storedTitle));
  ASSERT_TRUE(index.readSourceAuthor(aRecord, storedAuthor));
  EXPECT_EQ(storedTitle, title);
  EXPECT_EQ(storedAuthor, author);

  ClixRecord bRecord{};
  ASSERT_TRUE(recordAtPath(index, "/b.epub", bRecord));
  ASSERT_TRUE(index.readTitle(bRecord, storedTitle));
  ASSERT_TRUE(index.readSourceAuthor(bRecord, storedAuthor));
  EXPECT_EQ(storedTitle, emojiTitle);
  EXPECT_EQ(storedAuthor, emojiAuthor);
}

TEST_F(LibraryBuilderTest, SeriesAndGenreSurviveRebuildWithoutReparsing) {
  bookMetadata["/a.epub"].series = "Earthsea";
  bookMetadata["/a.epub"].seriesIndex = "2.5";
  bookMetadata["/a.epub"].genre = "Fantasy";
  ASSERT_TRUE(buildLibraryIndex("/", stats, true));
  fake::parses = 0;
  ASSERT_TRUE(buildLibraryIndex("/", stats, true));
  EXPECT_EQ(fake::parses, 0u);

  LibraryIndexFile index;
  ASSERT_TRUE(index.open(INDEX));
  ClixRecord record{};
  ASSERT_TRUE(recordAtPath(index, "/a.epub", record));
  std::string series;
  std::string genre;
  ASSERT_TRUE(index.readSeries(record, series));
  ASSERT_TRUE(index.readGenre(record, genre));
  EXPECT_EQ(series, "Earthsea");
  EXPECT_EQ(genre, "Fantasy");
  uint32_t position = 0;
  ASSERT_TRUE(index.readSeriesPosition(record, position));
  EXPECT_NE(position, CLIX_UNKNOWN_SERIES_POSITION);
}

TEST_F(LibraryBuilderTest, SeriesSortUsesNumericOrderThenTitleAndPutsMissingNumbersLast) {
  fake::add("/c.epub");
  fake::add("/d.epub");
  fake::add("/e.epub");
  fake::add("/f.epub");
  fake::add("/g.epub");
  for (const char* path : {"/a.epub", "/b.epub", "/c.epub", "/d.epub", "/e.epub", "/f.epub", "/g.epub"})
    bookMetadata[path].series = "Earthsea";
  bookMetadata["/a.epub"].seriesIndex = "10";
  bookMetadata["/b.epub"].seriesIndex = "2.5";
  bookMetadata["/c.epub"].seriesIndex = "2";
  bookMetadata["/d.epub"].seriesIndex = "invalid";
  bookMetadata["/e.epub"].seriesIndex = "2";
  bookMetadata["/f.epub"].seriesIndex = "-1";
  bookMetadata["/g.epub"].seriesIndex = "0";
  ASSERT_TRUE(buildLibraryIndex("/", stats, true));
  LibraryIndexFile index;
  ASSERT_TRUE(index.open(INDEX));
  EXPECT_EQ(pathAt(index, SortOrder::SeriesAsc, 0), "/f.epub");
  EXPECT_EQ(pathAt(index, SortOrder::SeriesAsc, 1), "/g.epub");
  EXPECT_EQ(pathAt(index, SortOrder::SeriesAsc, 2), "/c.epub");
  EXPECT_EQ(pathAt(index, SortOrder::SeriesAsc, 3), "/e.epub");
  EXPECT_EQ(pathAt(index, SortOrder::SeriesAsc, 4), "/b.epub");
  EXPECT_EQ(pathAt(index, SortOrder::SeriesAsc, 5), "/a.epub");
  EXPECT_EQ(pathAt(index, SortOrder::SeriesAsc, 6), "/d.epub");
}

TEST_F(LibraryBuilderTest, SeriesAndGenreSortByFullFoldedNameWithMissingValuesLast) {
  fake::add("/c.epub");
  fake::add("/d.epub");
  bookMetadata["/a.epub"].series = "Alpha";
  bookMetadata["/a.epub"].genre = "Zeta";
  bookMetadata["/b.epub"].series = "beta";
  bookMetadata["/b.epub"].genre = "Fantasy";
  bookMetadata["/c.epub"].series = "ALPHA";
  bookMetadata["/c.epub"].genre = "fantasy";
  ASSERT_TRUE(buildLibraryIndex("/", stats, true));

  LibraryIndexFile index;
  ASSERT_TRUE(index.open(INDEX));
  EXPECT_EQ(pathAt(index, SortOrder::SeriesAsc, 0), "/a.epub");
  EXPECT_EQ(pathAt(index, SortOrder::SeriesAsc, 1), "/c.epub");
  EXPECT_EQ(pathAt(index, SortOrder::SeriesAsc, 2), "/b.epub");
  EXPECT_EQ(pathAt(index, SortOrder::SeriesAsc, 3), "/d.epub");
  EXPECT_EQ(pathAt(index, SortOrder::SeriesDesc, 0), "/d.epub");
  EXPECT_EQ(pathAt(index, SortOrder::GenreAsc, 0), "/b.epub");
  EXPECT_EQ(pathAt(index, SortOrder::GenreAsc, 1), "/c.epub");
  EXPECT_EQ(pathAt(index, SortOrder::GenreAsc, 2), "/a.epub");
  EXPECT_EQ(pathAt(index, SortOrder::GenreAsc, 3), "/d.epub");
}

TEST_F(LibraryBuilderTest, SeriesSortRefinesLongSharedPrefixes) {
  const std::string prefix(24, 'a');
  bookMetadata["/a.epub"].series = prefix + "z";
  bookMetadata["/b.epub"].series = prefix + "b";
  ASSERT_TRUE(buildLibraryIndex("/", stats, true));
  LibraryIndexFile index;
  ASSERT_TRUE(index.open(INDEX));
  EXPECT_EQ(pathAt(index, SortOrder::SeriesAsc, 0), "/b.epub");
  EXPECT_EQ(pathAt(index, SortOrder::SeriesAsc, 1), "/a.epub");
}

TEST_F(LibraryBuilderTest, SeriesPositionSortsAfterANameLongerThanTheKeySegment) {
  const std::string series(48, 'a');
  bookMetadata["/a.epub"].series = series;
  bookMetadata["/b.epub"].series = series;
  bookMetadata["/a.epub"].seriesIndex = "10";
  bookMetadata["/b.epub"].seriesIndex = "2";
  ASSERT_TRUE(buildLibraryIndex("/", stats, true));
  LibraryIndexFile index;
  ASSERT_TRUE(index.open(INDEX));
  EXPECT_EQ(pathAt(index, SortOrder::SeriesAsc, 0), "/b.epub");
  EXPECT_EQ(pathAt(index, SortOrder::SeriesAsc, 1), "/a.epub");
}

TEST_F(LibraryBuilderTest, VersionThreeIndexReparsesSeriesOrderDuringUpgrade) {
  bookMetadata["/a.epub"].series = "Earthsea";
  initial();
  LibraryIndexFile before;
  ASSERT_TRUE(before.open(INDEX));
  ClixRecord original{};
  ASSERT_TRUE(recordAtPath(before, "/a.epub", original));
  before.close();

  // Two-book v3 and v6 indexes have the same aligned nameStart. The v3
  // permutation section ends early, leaving padding before the name blob.
  fake::files[INDEX]->bytes[offsetof(ClixHeader, formatVersion)] = 3;
  fake::parses = 0;
  ASSERT_TRUE(buildLibraryIndex("/", stats, true));
  EXPECT_EQ(fake::parses, 2u);
  EXPECT_EQ(stats.metadataReused, 0);
  LibraryIndexFile after;
  ASSERT_TRUE(after.open(INDEX));
  EXPECT_EQ(after.header().formatVersion, CLIX_FORMAT_VERSION);
  ClixRecord rebuilt{};
  ASSERT_TRUE(recordAtPath(after, "/a.epub", rebuilt));
  EXPECT_EQ(rebuilt.firstSeen, original.firstSeen);
  std::string series;
  ASSERT_TRUE(after.readSeries(rebuilt, series));
  EXPECT_EQ(series, "Earthsea");
}

TEST_F(LibraryBuilderTest, VersionFiveIndexReparsesSeriesPositionsAndKeepsArrivalHistory) {
  bookMetadata["/a.epub"].series = "Earthsea";
  bookMetadata["/b.epub"].series = "Earthsea";
  bookMetadata["/a.epub"].seriesIndex = "2";
  bookMetadata["/b.epub"].seriesIndex = "1";
  initial();
  LibraryIndexFile before;
  ASSERT_TRUE(before.open(INDEX));
  ClixRecord original{};
  ASSERT_TRUE(recordAtPath(before, "/a.epub", original));
  before.close();

  ASSERT_TRUE(downgradeIndexToVersionFive());
  fake::parses = 0;
  ASSERT_TRUE(buildLibraryIndex("/", stats, true));
  EXPECT_EQ(fake::parses, 2u);
  LibraryIndexFile after;
  ASSERT_TRUE(after.open(INDEX));
  EXPECT_EQ(pathAt(after, SortOrder::SeriesAsc, 0), "/b.epub");
  ClixRecord rebuilt{};
  ASSERT_TRUE(recordAtPath(after, "/a.epub", rebuilt));
  EXPECT_EQ(rebuilt.firstSeen, original.firstSeen);
}

TEST_F(LibraryBuilderTest, InterruptedUpgradeKeepsValidOldLiveIndexOverStaleBackup) {
  initial();
  LibraryIndexFile before;
  ASSERT_TRUE(before.open(INDEX));
  ClixRecord original{};
  ASSERT_TRUE(recordAtPath(before, "/a.epub", original));
  before.close();
  ASSERT_TRUE(downgradeIndexToVersionFive());

  constexpr char BACKUP[] = "/.crosspoint/library.bak";
  fake::files[BACKUP] = std::make_shared<fake::Node>(*fake::files[INDEX]);
  ClixHeader backupHeader{};
  std::memcpy(&backupHeader, fake::files[BACKUP]->bytes.data(), sizeof(backupHeader));
  for (uint16_t i = 0; i < backupHeader.bookCount; i++) {
    ClixRecord backupRecord{};
    std::memcpy(&backupRecord, fake::files[BACKUP]->bytes.data() + recordOffset(backupHeader, i), sizeof(backupRecord));
    backupRecord.firstSeen = 100 + i;
    std::memcpy(fake::files[BACKUP]->bytes.data() + recordOffset(backupHeader, i), &backupRecord, sizeof(backupRecord));
  }

  ASSERT_TRUE(buildLibraryIndex("/", stats, true));
  EXPECT_FALSE(Storage.exists(BACKUP));
  LibraryIndexFile after;
  ASSERT_TRUE(after.open(INDEX));
  ClixRecord rebuilt{};
  ASSERT_TRUE(recordAtPath(after, "/a.epub", rebuilt));
  EXPECT_EQ(rebuilt.firstSeen, original.firstSeen);
}

TEST_F(LibraryBuilderTest, OldFoldVersionRebuildsTitleKeysWithoutReparsingBooks) {
  bookMetadata["/a.epub"].title = "The Iliad";
  bookMetadata["/b.epub"].title = "Rendezvous";
  initial();
  LibraryIndexFile before;
  ASSERT_TRUE(before.open(INDEX));
  ASSERT_EQ(pathAt(before, SortOrder::TitleAsc, 1), "/a.epub");
  ClixRecord original{};
  ClixRecord other{};
  ASSERT_TRUE(recordAtPath(before, "/a.epub", original));
  ASSERT_TRUE(recordAtPath(before, "/b.epub", other));
  before.close();

  // Simulate the old index's "the " article stripping without changing its
  // stored title metadata. The upgrade must build the new key from that title.
  auto& bytes = fake::files[INDEX]->bytes;
  ClixHeader header{};
  std::memcpy(&header, bytes.data(), sizeof(header));
  ClixRecord oldRecord = original;
  constexpr char oldKey[] = "iliad";
  oldRecord.foldLen = sizeof(oldKey) - 1;
  std::memset(oldRecord.fold, 0, sizeof(oldRecord.fold));
  std::memcpy(oldRecord.fold, oldKey, oldRecord.foldLen);
  // Old title order is "iliad" before "rendezvous"; the new key puts "the" after it.
  std::memcpy(bytes.data() + recordOffset(header, 0), &oldRecord, sizeof(oldRecord));
  std::memcpy(bytes.data() + recordOffset(header, 1), &other, sizeof(other));
  bytes[offsetof(ClixHeader, foldVersion)] = CLIX_FOLD_VERSION - 1;
  LibraryIndexFile stale;
  ASSERT_TRUE(stale.openForReconciliation(INDEX));
  EXPECT_EQ(pathAt(stale, SortOrder::TitleAsc, 0), "/a.epub");
  stale.close();

  fake::parses = 0;
  ASSERT_TRUE(buildLibraryIndex("/", stats, true));
  EXPECT_EQ(fake::parses, 0u);
  EXPECT_EQ(stats.metadataReused, 2);
  EXPECT_TRUE(stats.indexReplaced);
  LibraryIndexFile after;
  ASSERT_TRUE(after.open(INDEX));
  EXPECT_EQ(after.header().foldVersion, CLIX_FOLD_VERSION);
  EXPECT_EQ(pathAt(after, SortOrder::TitleAsc, 0), "/b.epub");
  EXPECT_EQ(pathAt(after, SortOrder::TitleAsc, 1), "/a.epub");
  ClixRecord rebuilt{};
  ASSERT_TRUE(recordAtPath(after, "/a.epub", rebuilt));
  EXPECT_EQ(std::string(rebuilt.fold, rebuilt.foldLen), "the iliad");
  EXPECT_EQ(foldedGroupInitial(std::string_view(rebuilt.fold, rebuilt.foldLen)), static_cast<uint32_t>('t'));
  EXPECT_EQ(rebuilt.firstSeen, original.firstSeen);
}

TEST_F(LibraryBuilderTest, VersionFourIndexKeepsFirstSeenDuringUpgrade) {
  initial();
  LibraryIndexFile before;
  ASSERT_TRUE(before.open(INDEX));
  ClixRecord original{};
  ASSERT_TRUE(recordAtPath(before, "/a.epub", original));
  before.close();

  // For two books, the v4 and v6 side arrays fit before the same aligned
  // name section. This models an old index without changing its record data.
  fake::files[INDEX]->bytes[offsetof(ClixHeader, formatVersion)] = 4;
  fake::files[INDEX]->bytes[offsetof(ClixHeader, foldVersion)] = 1;
  fake::parses = 0;
  ASSERT_TRUE(buildLibraryIndex("/", stats, true));
  EXPECT_EQ(fake::parses, 2u);
  EXPECT_EQ(stats.metadataReused, 0);
  EXPECT_TRUE(stats.indexReplaced);

  LibraryIndexFile after;
  ASSERT_TRUE(after.open(INDEX));
  EXPECT_EQ(after.header().formatVersion, CLIX_FORMAT_VERSION);
  EXPECT_EQ(after.header().foldVersion, CLIX_FOLD_VERSION);
  ClixRecord rebuilt{};
  ASSERT_TRUE(recordAtPath(after, "/a.epub", rebuilt));
  EXPECT_EQ(rebuilt.firstSeen, original.firstSeen);
  EXPECT_EQ(rebuilt.modificationTime, original.modificationTime);
}

TEST_F(LibraryBuilderTest, InterruptedUpgradeRestoresVersionThreeBackup) {
  bookMetadata["/a.epub"].series = "Earthsea";
  initial();
  LibraryIndexFile before;
  ASSERT_TRUE(before.open(INDEX));
  ClixRecord original{};
  ASSERT_TRUE(recordAtPath(before, "/a.epub", original));
  before.close();

  constexpr char BACKUP[] = "/.crosspoint/library.bak";
  fake::files[BACKUP] = std::make_shared<fake::Node>(*fake::files[INDEX]);
  fake::files[BACKUP]->bytes[offsetof(ClixHeader, formatVersion)] = 3;
  fake::files[INDEX]->bytes[0] = 'X';  // Damaged live index after install.
  fake::parses = 0;

  ASSERT_TRUE(buildLibraryIndex("/", stats, true));
  EXPECT_EQ(fake::parses, 2u);
  EXPECT_EQ(stats.metadataReused, 0);
  EXPECT_FALSE(Storage.exists(BACKUP));
  LibraryIndexFile after;
  ASSERT_TRUE(after.open(INDEX));
  ClixRecord rebuilt{};
  ASSERT_TRUE(recordAtPath(after, "/a.epub", rebuilt));
  EXPECT_EQ(rebuilt.firstSeen, original.firstSeen);
}

TEST_F(LibraryBuilderTest, FailedUpgradeKeepsVersionThreeShelfReadable) {
  bookMetadata["/a.epub"].series = "Earthsea";
  initial();
  fake::files[INDEX]->bytes[offsetof(ClixHeader, formatVersion)] = 3;
  fake::failWritePath = "/.crosspoint/library.new";

  EXPECT_FALSE(buildLibraryIndex("/", stats, true));
  LibraryIndexFile shelf;
  EXPECT_FALSE(shelf.open(INDEX));
  ASSERT_TRUE(shelf.openForReconciliation(INDEX));
  EXPECT_EQ(shelf.bookCount(), 2);
  EXPECT_EQ(pathAt(shelf, SortOrder::TitleAsc, 0), "/a.epub");
  ClixRecord record{};
  ASSERT_TRUE(recordAtPath(shelf, "/a.epub", record));
  std::string series;
  ASSERT_TRUE(shelf.readSeries(record, series));
  EXPECT_EQ(series, "Earthsea");
}

TEST_F(LibraryBuilderTest, VersionTwoIndexRebuildKeepsFirstSeenOrder) {
  initial();
  LibraryIndexFile before;
  ASSERT_TRUE(before.open(INDEX));
  ClixRecord original{};
  ASSERT_TRUE(recordAtPath(before, "/a.epub", original));
  before.close();

  // The old format has the same header and record stride. Reconciliation only
  // needs those records and path hashes; its shorter metadata blob is replaced.
  fake::files[INDEX]->bytes[offsetof(ClixHeader, formatVersion)] = 2;
  fake::parses = 0;
  ASSERT_TRUE(buildLibraryIndex("/", stats, true));
  EXPECT_EQ(fake::parses, 2u);

  LibraryIndexFile after;
  ASSERT_TRUE(after.open(INDEX));
  EXPECT_EQ(after.header().formatVersion, CLIX_FORMAT_VERSION);
  ClixRecord rebuilt{};
  ASSERT_TRUE(recordAtPath(after, "/a.epub", rebuilt));
  EXPECT_EQ(rebuilt.firstSeen, original.firstSeen);
}

TEST_F(LibraryBuilderTest, ZeroTimestampAndFailedExtractionAreNeverFresh) {
  fake::files["/a.epub"]->time = 0;
  bookMetadata["/b.epub"].success = false;
  initial();
  fake::parses = 0;

  ASSERT_TRUE(buildLibraryIndex("/", stats, true));

  EXPECT_EQ(fake::parses, 2u);
  EXPECT_EQ(stats.metadataReused, 0);
  EXPECT_TRUE(stats.indexReplaced);
}

TEST_F(LibraryBuilderTest, ZeroTimestampDoesNotReuseStaleEpubCache) {
  fake::files["/a.epub"]->time = 0;
  bookMetadata["/a.epub"].title = "Original title";
  initial();

  cachedBookMetadata["/a.epub"].title = "Original title";
  bookMetadata["/a.epub"].title = "Replacement title";
  metadataCacheUse.clear();
  ASSERT_TRUE(buildLibraryIndex("/", stats, true));

  ASSERT_FALSE(metadataCacheUse.empty());
  EXPECT_FALSE(metadataCacheUse.front());
  LibraryIndexFile index;
  ASSERT_TRUE(index.open(INDEX));
  ClixRecord record{};
  ASSERT_TRUE(recordAtPath(index, "/a.epub", record));
  std::string title;
  ASSERT_TRUE(index.readTitle(record, title));
  EXPECT_EQ(title, "Replacement title");
}

TEST_F(LibraryBuilderTest, MetadataModeChangesInvalidateCachedMetadata) {
  initial();
  fake::parses = 0;

  ASSERT_TRUE(buildLibraryIndex("/", stats, false));
  EXPECT_EQ(fake::parses, 0u);
  LibraryIndexFile index;
  ASSERT_TRUE(index.open(INDEX));
  EXPECT_EQ(index.header().metadataEnabled, 0);
  index.close();

  ASSERT_TRUE(buildLibraryIndex("/", stats, true));
  EXPECT_EQ(fake::parses, 2u);
}

TEST_F(LibraryBuilderTest, RebuildVotesFromSourceAuthorInsteadOfPriorCanonicalAuthor) {
  fake::add("/c.epub");
  bookMetadata["/a.epub"].author = "Victor Hugo";
  bookMetadata["/b.epub"].author = "Hugo Victor";
  bookMetadata["/c.epub"].author = "Hugo Victor";
  initial();
  ASSERT_TRUE(Storage.remove("/b.epub"));
  ASSERT_TRUE(Storage.remove("/c.epub"));
  fake::parses = 0;

  ASSERT_TRUE(buildLibraryIndex("/", stats, true));

  EXPECT_EQ(fake::parses, 0u);
  LibraryIndexFile index;
  ASSERT_TRUE(index.open(INDEX));
  ClixRecord record{};
  std::string author;
  ASSERT_TRUE(index.readRecord(0, record));
  ASSERT_TRUE(index.readAuthor(record, author));
  EXPECT_EQ(author, "Victor Hugo");
}

TEST_F(LibraryBuilderTest, SortsPastTheFirstTwelveTitleAndSurnameBytes) {
  const std::string commonPrefix(36, 'Q');
  bookMetadata["/a.epub"].title = commonPrefix + " Z";
  bookMetadata["/b.epub"].title = commonPrefix + " A";
  bookMetadata["/a.epub"].author = "Alice " + commonPrefix + "Z";
  bookMetadata["/b.epub"].author = "Bob " + commonPrefix + "A";

  initial();
  LibraryIndexFile index;
  ASSERT_TRUE(index.open(INDEX));
  EXPECT_EQ(pathAt(index, SortOrder::TitleAsc, 0), "/b.epub");
  EXPECT_EQ(pathAt(index, SortOrder::TitleAsc, 1), "/a.epub");
  EXPECT_EQ(pathAt(index, SortOrder::AuthorAsc, 0), "/b.epub");
  EXPECT_EQ(pathAt(index, SortOrder::AuthorAsc, 1), "/a.epub");
}

TEST_F(LibraryBuilderTest, FirstNameAndLastNameAuthorOrdersDiffer) {
  fake::add("/c.epub");
  bookMetadata["/a.epub"].author = "Zoe Adams";
  bookMetadata["/b.epub"].author = "Amy Young";
  bookMetadata["/c.epub"].author = "Beth Moore";
  initial();

  LibraryIndexFile index;
  ASSERT_TRUE(index.open(INDEX));
  EXPECT_EQ(pathAt(index, SortOrder::AuthorAsc, 0), "/a.epub");
  EXPECT_EQ(pathAt(index, SortOrder::AuthorAsc, 1), "/c.epub");
  EXPECT_EQ(pathAt(index, SortOrder::AuthorAsc, 2), "/b.epub");
  EXPECT_EQ(pathAt(index, SortOrder::AuthorFirstAsc, 0), "/b.epub");
  EXPECT_EQ(pathAt(index, SortOrder::AuthorFirstAsc, 1), "/c.epub");
  EXPECT_EQ(pathAt(index, SortOrder::AuthorFirstAsc, 2), "/a.epub");
  EXPECT_EQ(pathAt(index, SortOrder::AuthorFirstDesc, 0), "/a.epub");
}

TEST_F(LibraryBuilderTest, FirstNameSortRefinesLongSharedPrefixes) {
  const std::string prefix(30, 'Q');
  bookMetadata["/a.epub"].author = prefix + " Zoe";
  bookMetadata["/b.epub"].author = prefix + " Amy";
  initial();

  LibraryIndexFile index;
  ASSERT_TRUE(index.open(INDEX));
  EXPECT_EQ(pathAt(index, SortOrder::AuthorFirstAsc, 0), "/b.epub");
  EXPECT_EQ(pathAt(index, SortOrder::AuthorFirstAsc, 1), "/a.epub");
}

TEST_F(LibraryBuilderTest, EqualBasenamesInDifferentFoldersReconcileIndependently) {
  fake::add("/one/same.epub");
  fake::add("/two/same.epub");
  bookMetadata["/one/same.epub"].title = "One";
  bookMetadata["/two/same.epub"].title = "Two";
  initial();
  fake::files["/two/same.epub"]->time++;
  fake::parses = 0;

  ASSERT_TRUE(buildLibraryIndex("/", stats, true));

  EXPECT_EQ(fake::parses, 1u);
  EXPECT_EQ(stats.metadataReused, 3);
}

TEST_F(LibraryBuilderTest, DateAddedUsesCreationTimeRatherThanModificationTime) {
  // Modification dates point in the opposite direction. Books with equal
  // creation times retain their first-seen order.
  fake::add("/c.epub", "book c", /*time=*/9);
  fake::add("/d.epub", "book d", /*time=*/0);
  fake::files["/c.epub"]->created = 0;
  fake::files["/d.epub"]->created = 9;
  ASSERT_TRUE(buildLibraryIndex("/", stats, true));

  LibraryIndexFile index;
  ASSERT_TRUE(index.open(INDEX));
  EXPECT_EQ(pathAt(index, SortOrder::RecentAsc, 0), "/c.epub");
  EXPECT_EQ(pathAt(index, SortOrder::RecentAsc, 1), "/a.epub");
  EXPECT_EQ(pathAt(index, SortOrder::RecentAsc, 2), "/b.epub");
  EXPECT_EQ(pathAt(index, SortOrder::RecentAsc, 3), "/d.epub");
  EXPECT_EQ(pathAt(index, SortOrder::RecentDesc, 0), "/d.epub");
  uint32_t created = 0;
  EXPECT_TRUE(index.readCreationTime(index.ordinalForRow(SortOrder::RecentAsc, 0), created));
  EXPECT_EQ(created, 0u);
}

TEST_F(LibraryBuilderTest, MissingCreationTimesFallBackToFirstSeenAcrossRebuilds) {
  fake::files["/a.epub"]->created = 0;
  fake::files["/b.epub"]->created = 0;
  initial();
  fake::add("/c.epub", "book c", /*time=*/100);
  fake::files["/c.epub"]->created = 0;
  ASSERT_TRUE(buildLibraryIndex("/", stats, true));
  LibraryIndexFile index;
  ASSERT_TRUE(index.open(INDEX));
  EXPECT_EQ(pathAt(index, SortOrder::RecentAsc, 0), "/a.epub");
  EXPECT_EQ(pathAt(index, SortOrder::RecentAsc, 1), "/b.epub");
  EXPECT_EQ(pathAt(index, SortOrder::RecentAsc, 2), "/c.epub");
}

TEST_F(LibraryBuilderTest, CreationTimeChangeUpdatesOrderWithoutReparsingMetadata) {
  initial();
  fake::files["/a.epub"]->created = 2;
  fake::parses = 0;
  ASSERT_TRUE(buildLibraryIndex("/", stats, true));
  EXPECT_EQ(fake::parses, 0u);
  EXPECT_EQ(stats.metadataReused, 2);
  EXPECT_TRUE(stats.indexReplaced);

  LibraryIndexFile index;
  ASSERT_TRUE(index.open(INDEX));
  EXPECT_EQ(pathAt(index, SortOrder::RecentAsc, 0), "/b.epub");
  EXPECT_EQ(pathAt(index, SortOrder::RecentAsc, 1), "/a.epub");
  uint32_t created = 0;
  ASSERT_TRUE(index.readCreationTime(index.ordinalForRow(SortOrder::RecentAsc, 1), created));
  EXPECT_EQ(created, 2u);
}

TEST_F(LibraryBuilderTest, CreationSortAllocationFailureRetriesOnNextScan) {
  bool foundArrivalFallback = false;
  // Find the fallible creation-time array without coupling this test to the
  // exact allocation order of the other builder phases.
  for (int failAt = 0; failAt < 32 && !foundArrivalFallback; failAt++) {
    fake::reset();
    fake::add("/a.txt");
    fake::add("/b.txt");
    fake::files["/a.txt"]->created = 9;
    fake::files["/b.txt"]->created = 1;
    fake::failAlloc = failAt;
    if (!buildLibraryIndex("/", stats, false)) continue;

    LibraryIndexFile index;
    ASSERT_TRUE(index.open(INDEX));
    foundArrivalFallback = (index.header().flags & CLIX_FLAG_ARRIVAL_DEGRADED) != 0;
    if (!foundArrivalFallback) continue;
    EXPECT_TRUE(libraryIndexNeedsRefresh());
    EXPECT_FALSE(stats.ranksDegraded);
    EXPECT_EQ(pathAt(index, SortOrder::RecentAsc, 0), "/a.txt");
    index.close();

    ASSERT_TRUE(buildLibraryIndex("/", stats, false));
    EXPECT_TRUE(stats.indexReplaced);
    ASSERT_TRUE(index.open(INDEX));
    EXPECT_EQ(index.header().flags & CLIX_FLAG_ARRIVAL_DEGRADED, 0);
    EXPECT_EQ(pathAt(index, SortOrder::RecentAsc, 0), "/b.txt");
  }
  EXPECT_TRUE(foundArrivalFallback);
}

TEST_F(LibraryBuilderTest, AddedRemovedMovedAndRenamedBooksKeepArrivalOrder) {
  initial();
  fake::add("/c.epub");
  ASSERT_TRUE(buildLibraryIndex("/", stats, true));

  LibraryIndexFile index;
  ASSERT_TRUE(index.open(INDEX));
  EXPECT_EQ(pathAt(index, SortOrder::RecentAsc, 0), "/a.epub");
  EXPECT_EQ(pathAt(index, SortOrder::RecentAsc, 1), "/b.epub");
  EXPECT_EQ(pathAt(index, SortOrder::RecentAsc, 2), "/c.epub");
  index.close();

  ASSERT_TRUE(Storage.remove("/b.epub"));
  ASSERT_TRUE(Storage.rename("/a.epub", "/moved.epub"));
  ASSERT_TRUE(buildLibraryIndex("/", stats, true));
  EXPECT_EQ(stats.removed, 1);
  EXPECT_EQ(stats.renamed, 1);
  ASSERT_TRUE(index.open(INDEX));
  EXPECT_EQ(pathAt(index, SortOrder::RecentAsc, 0), "/moved.epub");
  EXPECT_EQ(pathAt(index, SortOrder::RecentAsc, 1), "/c.epub");
  index.close();

  ASSERT_TRUE(Storage.rename("/moved.epub", "/renamed.epub"));
  ASSERT_TRUE(buildLibraryIndex("/", stats, true));
  EXPECT_EQ(stats.renamed, 1);
  ASSERT_TRUE(index.open(INDEX));
  EXPECT_EQ(pathAt(index, SortOrder::RecentAsc, 0), "/renamed.epub");
  EXPECT_EQ(pathAt(index, SortOrder::RecentAsc, 1), "/c.epub");
}

TEST_F(LibraryBuilderTest, WholeFolderRenameWithUniqueSizePreservesArrivalOrder) {
  fake::add("/old/unique.epub", "a uniquely sized book");
  initial();
  ASSERT_TRUE(Storage.mkdir("/new"));
  ASSERT_TRUE(Storage.rename("/old/unique.epub", "/new/unique.epub"));
  fake::parses = 0;

  ASSERT_TRUE(buildLibraryIndex("/", stats, true));

  EXPECT_EQ(stats.renamed, 1);
  EXPECT_EQ(stats.removed, 0);
  EXPECT_EQ(fake::parses, 1u);
  LibraryIndexFile index;
  ASSERT_TRUE(index.open(INDEX));
  EXPECT_EQ(pathAt(index, SortOrder::RecentAsc, 2), "/new/unique.epub");
}

TEST_F(LibraryBuilderTest, DuplicateDetectionRemainsBoundedAndFindsTrackedKeysAfterTheCap) {
  fake::duplicateDirectoryEntry("/a.epub");
  ASSERT_TRUE(buildLibraryIndex("/", stats, true));
  EXPECT_EQ(stats.books, 2);
  EXPECT_EQ(stats.duplicatesDropped, 1);
  EXPECT_FALSE(stats.dedupDegraded);

  fake::reset();
  bookMetadata.clear();
  for (unsigned i = 0; i <= LIBRARY_MAX_DEDUP_KEYS; i++) {
    fake::add("/book" + numbered("", i) + ".txt");
  }
  fake::duplicateDirectoryEntry("/book0000.txt");
  ASSERT_TRUE(buildLibraryIndex("/", stats, false));
  EXPECT_EQ(stats.books, LIBRARY_MAX_DEDUP_KEYS + 1);
  EXPECT_EQ(stats.duplicatesDropped, 1);
  EXPECT_TRUE(stats.dedupDegraded);
  EXPECT_LT(fake::delays, 2000u);
}

TEST_F(LibraryBuilderTest, ReadWriteCloseAndAllocationFailuresRetainPreviousIndex) {
  initial();
  const auto old = fake::files[INDEX]->bytes;

  fake::failRead = 0;
  EXPECT_FALSE(buildLibraryIndex("/", stats, true));
  EXPECT_EQ(fake::files[INDEX]->bytes, old);
  fake::failRead = -1;

  fake::files["/a.epub"]->time++;
  fake::failWrite = 0;
  EXPECT_FALSE(buildLibraryIndex("/", stats, true));
  EXPECT_EQ(fake::files[INDEX]->bytes, old);
  fake::failWrite = -1;

  fake::failWritePath = "/.crosspoint/library.new";
  EXPECT_FALSE(buildLibraryIndex("/", stats, true));
  EXPECT_EQ(fake::files[INDEX]->bytes, old);

  fake::failClosePath = "/.crosspoint/library.new";
  EXPECT_FALSE(buildLibraryIndex("/", stats, true));
  EXPECT_EQ(fake::files[INDEX]->bytes, old);

  fake::failAlloc = 3;
  EXPECT_FALSE(buildLibraryIndex("/", stats, true));
  EXPECT_EQ(fake::files[INDEX]->bytes, old);
  fake::failAlloc = -1;

  fake::failRename = 1;
  EXPECT_FALSE(buildLibraryIndex("/", stats, true));
  EXPECT_EQ(fake::files[INDEX]->bytes, old);
}

TEST_F(LibraryBuilderTest, TruncatedPersistedPathHashAbortsAndRetainsTheLiveIndex) {
  initial();
  auto& bytes = fake::files[INDEX]->bytes;
  ClixHeader header{};
  std::memcpy(&header, bytes.data(), sizeof(header));
  ClixRecord record{};
  std::memcpy(&record, bytes.data() + recordOffset(header, 0), sizeof(record));
  record.nameOff = header.nameLen - 4;
  std::memcpy(bytes.data() + recordOffset(header, 0), &record, sizeof(record));
  const auto corrupted = bytes;

  EXPECT_FALSE(buildLibraryIndex("/", stats, true));
  EXPECT_EQ(fake::files[INDEX]->bytes, corrupted);
  EXPECT_FALSE(Storage.exists("/.crosspoint/library.stage"));
  EXPECT_FALSE(Storage.exists("/.crosspoint/library.stage.f"));
}

TEST_F(LibraryBuilderTest, LibrariesPastOldGateAndAtFormatCeilingKeepAllOrders) {
  for (const unsigned count : {513u, static_cast<unsigned>(CLIX_MAX_RECORDS)}) {
    fake::reset();
    bookMetadata.clear();
    std::vector<unsigned> authorOrder(count);
    std::iota(authorOrder.begin(), authorOrder.end(), 0u);
    for (unsigned i = 0; i < count; i++) {
      const std::string path = "/book" + numbered("", i) + ".epub";
      fake::add(path);
      bookMetadata[path].title = numbered("Title ", count - 1 - i);
      bookMetadata[path].author = numbered("Writer ", (i * (count == 513 ? 257u : 2053u)) % count);
    }

    ASSERT_TRUE(buildLibraryIndex("/", stats, true)) << count;
    ASSERT_EQ(stats.books, count);
    EXPECT_FALSE(stats.ranksDegraded);

    if (count == CLIX_MAX_RECORDS) {
      fake::parses = 0;
      fake::resetIoCounters();
      ASSERT_TRUE(buildLibraryIndex("/", stats, true));
      EXPECT_EQ(fake::parses, 0u);
      EXPECT_EQ(stats.metadataReused, CLIX_MAX_RECORDS);
      // The fixed-size per-directory duplicate tracker is deliberately bounded
      // below the maximum library size, so this index remains degraded. It must
      // rebuild rather than silently preserve an old degraded header.
      EXPECT_TRUE(stats.indexReplaced);
      EXPECT_TRUE(stats.dedupDegraded);
      EXPECT_LT(fake::delays, 10000u);
    }

    std::sort(authorOrder.begin(), authorOrder.end(), [count](const unsigned a, const unsigned b) {
      return (a * (count == 513 ? 257u : 2053u)) % count < (b * (count == 513 ? 257u : 2053u)) % count;
    });
    LibraryIndexFile index;
    ASSERT_TRUE(index.open(INDEX));
    for (uint16_t row = 0; row < count; row++) {
      EXPECT_EQ(pathAt(index, SortOrder::RecentAsc, row), "/book" + numbered("", row) + ".epub") << count << ':' << row;
      EXPECT_EQ(pathAt(index, SortOrder::TitleAsc, row), "/book" + numbered("", count - 1 - row) + ".epub")
          << count << ':' << row;
      EXPECT_EQ(pathAt(index, SortOrder::AuthorAsc, row), "/book" + numbered("", authorOrder[row]) + ".epub")
          << count << ':' << row;
      EXPECT_EQ(pathAt(index, SortOrder::AuthorFirstAsc, row), "/book" + numbered("", authorOrder[row]) + ".epub")
          << count << ':' << row;
    }
  }
}

TEST_F(LibraryBuilderTest, BookPastFormatCeilingKeepsPreviousIndex) {
  initial();
  const auto previous = fake::files[INDEX]->bytes;
  for (unsigned i = 0; i < CLIX_MAX_RECORDS - 1; i++) {
    fake::add("/book" + numbered("", i) + ".epub");
  }

  EXPECT_FALSE(buildLibraryIndex("/", stats, false));
  EXPECT_EQ(fake::files[INDEX]->bytes, previous);
  EXPECT_FALSE(Storage.exists("/.crosspoint/library.stage"));
  EXPECT_FALSE(Storage.exists("/.crosspoint/library.stage.f"));
}

TEST_F(LibraryBuilderTest, SortAllocationFailureProducesValidDegradedIndex) {
  fake::reset();
  for (unsigned i = 0; i < 513; i++) fake::add("/book" + numbered("", i) + ".txt");
  fake::failAlloc = 6;

  ASSERT_TRUE(buildLibraryIndex("/", stats, false));
  EXPECT_TRUE(fake::failureTriggered);
  EXPECT_TRUE(stats.ranksDegraded);
  EXPECT_TRUE(stats.indexReplaced);

  LibraryIndexFile index;
  ASSERT_TRUE(index.open(INDEX));
  EXPECT_EQ(index.bookCount(), 513);
  EXPECT_NE(index.header().flags & CLIX_FLAG_RANKS_DEGRADED, 0);
  index.close();

  ASSERT_TRUE(buildLibraryIndex("/", stats, false));
  EXPECT_TRUE(stats.indexReplaced);
  EXPECT_FALSE(stats.ranksDegraded);
  ASSERT_TRUE(index.open(INDEX));
  EXPECT_EQ(index.header().flags & CLIX_FLAG_RANKS_DEGRADED, 0);
  EXPECT_EQ(pathAt(index, SortOrder::TitleAsc, 0), "/book0000.txt");
}

TEST_F(LibraryBuilderTest, PriorDedupDegradationForcesReplacement) {
  initial();
  auto& bytes = fake::files[INDEX]->bytes;
  ClixHeader header{};
  std::memcpy(&header, bytes.data(), sizeof(header));
  header.flags |= CLIX_FLAG_DEDUP_DEGRADED;
  std::memcpy(bytes.data(), &header, sizeof(header));

  ASSERT_TRUE(buildLibraryIndex("/", stats, true));
  EXPECT_TRUE(stats.indexReplaced);
  LibraryIndexFile index;
  ASSERT_TRUE(index.open(INDEX));
  EXPECT_EQ(index.header().flags & CLIX_FLAG_DEDUP_DEGRADED, 0);
}

TEST_F(LibraryBuilderTest, DirtyIndexClearsOnSuccessAndRetriesAfterFailure) {
  EXPECT_TRUE(libraryIndexNeedsRefresh());
  initial();
  EXPECT_FALSE(libraryIndexNeedsRefresh());
  invalidateLibraryIndex();
  fake::failOpenPath = "/.crosspoint/library.idx";
  EXPECT_FALSE(buildLibraryIndex("/", stats, true));
  EXPECT_TRUE(libraryIndexNeedsRefresh());
  ASSERT_TRUE(buildLibraryIndex("/", stats, true));
  EXPECT_FALSE(libraryIndexNeedsRefresh());
}

TEST_F(LibraryBuilderTest, InvalidationDuringScanSurvivesSuccessfulPublish) {
  fake::onService = &invalidateLibraryIndex;
  initial();
  EXPECT_GT(fake::delays, 0u);
  EXPECT_TRUE(libraryIndexNeedsRefresh());
  fake::onService = nullptr;
  ASSERT_TRUE(buildLibraryIndex("/", stats, true));
  EXPECT_FALSE(libraryIndexNeedsRefresh());
}

TEST_F(LibraryBuilderTest, EmptyLibraryStillRecordsMetadataModeChanges) {
  fake::files.erase("/a.epub");
  fake::files.erase("/b.epub");
  ASSERT_TRUE(buildLibraryIndex("/", stats, false));
  ASSERT_TRUE(buildLibraryIndex("/", stats, true));
  LibraryIndexFile index;
  ASSERT_TRUE(index.open(INDEX));
  EXPECT_EQ(index.header().metadataEnabled, 1);
}
