#include <gtest/gtest.h>

#include <filesystem>
#include <string>
#include <vector>

#include "HalStorage.h"  // test stub: host file system
#include "HighlightStore.h"

namespace {

using HighlightFormat::CategoryType;
using HighlightFormat::Entry;

class HighlightStoreTest : public ::testing::Test {
 protected:
  void SetUp() override {
    const auto dir = std::filesystem::temp_directory_path() /
                     ("hlstore_" + std::string(::testing::UnitTest::GetInstance()->current_test_info()->name()));
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir / ".crosspoint");
    testRoot = dir.string();
    root_ = dir;
  }
  void TearDown() override { std::filesystem::remove_all(root_); }

  static Entry entry(const std::string& text, const std::string& meaning = "") {
    Entry e;
    e.text = text;
    e.meaning = meaning;
    return e;
  }

  std::filesystem::path root_;
};

TEST_F(HighlightStoreTest, CreatesDefaultCategoriesOnce) {
  HighlightStore::ensureDefaults();
  auto cats = HighlightStore::listCategories();
  ASSERT_EQ(cats.size(), 2u);
  EXPECT_EQ(cats[0].name, "Favorite Lines");
  EXPECT_EQ(cats[0].type, CategoryType::Notes);
  EXPECT_EQ(cats[1].name, "Flashcards");
  EXPECT_EQ(cats[1].type, CategoryType::Flashcards);

  // Deleting both must not bring them back: defaults are a first-run thing.
  EXPECT_TRUE(HighlightStore::deleteCategory(cats[0].id));
  EXPECT_TRUE(HighlightStore::deleteCategory(cats[1].id));
  HighlightStore::ensureDefaults();
  EXPECT_TRUE(HighlightStore::listCategories().empty());
}

TEST_F(HighlightStoreTest, AddReadAndCount) {
  const int id = HighlightStore::createCategory("Words", CategoryType::Flashcards);
  ASSERT_GT(id, 0);
  ASSERT_TRUE(HighlightStore::addEntry(id, entry("Häuser", "Haus\nhouse <n>")));
  ASSERT_TRUE(HighlightStore::addEntry(id, entry("gemacht", "machen\nto make")));
  ASSERT_TRUE(HighlightStore::addEntry(id, entry("Über", "")));

  HighlightStore::CategoryInfo info;
  ASSERT_TRUE(HighlightStore::findCategory(id, info));
  EXPECT_EQ(info.count, 3u);

  std::vector<uint32_t> offsets;
  ASSERT_TRUE(HighlightStore::loadEntryOffsets(id, offsets));
  ASSERT_EQ(offsets.size(), 3u);
  Entry e;
  ASSERT_TRUE(HighlightStore::readEntry(id, offsets[1], e));
  EXPECT_EQ(e.text, "gemacht");
  EXPECT_EQ(e.meaning, "machen\nto make");

  std::vector<Entry> window;
  ASSERT_TRUE(HighlightStore::readEntries(id, offsets, 1, 5, window));
  ASSERT_EQ(window.size(), 2u);
  EXPECT_EQ(window[0].text, "gemacht");
  EXPECT_EQ(window[1].text, "Über");
}

TEST_F(HighlightStoreTest, RenameKeepsEntries) {
  const int id = HighlightStore::createCategory("Old", CategoryType::Notes);
  ASSERT_TRUE(HighlightStore::addEntry(id, entry("A line to keep.")));
  ASSERT_TRUE(HighlightStore::renameCategory(id, "New Name"));
  HighlightStore::CategoryInfo info;
  ASSERT_TRUE(HighlightStore::findCategory(id, info));
  EXPECT_EQ(info.name, "New Name");
  EXPECT_EQ(info.count, 1u);
}

TEST_F(HighlightStoreTest, DeleteEntryKeepsLevelsAligned) {
  const int id = HighlightStore::createCategory("Cards", CategoryType::Flashcards);
  for (const char* w : {"eins", "zwei", "drei", "vier"}) ASSERT_TRUE(HighlightStore::addEntry(id, entry(w)));
  // Answer card 1 ("zwei") and card 3 ("vier").
  ASSERT_TRUE(HighlightStore::setLevel(id, 1, 3));
  ASSERT_TRUE(HighlightStore::setLevel(id, 3, 5));
  std::vector<uint8_t> levels;
  ASSERT_TRUE(HighlightStore::loadLevels(id, 4, levels));
  EXPECT_EQ(levels, (std::vector<uint8_t>{0, 3, 0, 5}));

  // Delete "eins": remaining cards keep their own levels.
  ASSERT_TRUE(HighlightStore::deleteEntry(id, 0));
  std::vector<uint32_t> offsets;
  ASSERT_TRUE(HighlightStore::loadEntryOffsets(id, offsets));
  ASSERT_EQ(offsets.size(), 3u);
  Entry e;
  ASSERT_TRUE(HighlightStore::readEntry(id, offsets[0], e));
  EXPECT_EQ(e.text, "zwei");
  ASSERT_TRUE(HighlightStore::loadLevels(id, 3, levels));
  EXPECT_EQ(levels, (std::vector<uint8_t>{3, 0, 5}));
}

TEST_F(HighlightStoreTest, SetLevelPadsUnansweredCards) {
  const int id = HighlightStore::createCategory("Cards", CategoryType::Flashcards);
  for (const char* w : {"a1", "b2", "c3"}) ASSERT_TRUE(HighlightStore::addEntry(id, entry(w)));
  ASSERT_TRUE(HighlightStore::setLevel(id, 2, 4));
  std::vector<uint8_t> levels;
  ASSERT_TRUE(HighlightStore::loadLevels(id, 3, levels));
  EXPECT_EQ(levels, (std::vector<uint8_t>{0, 0, 4}));
}

TEST_F(HighlightStoreTest, NewIdsFollowTheHighest) {
  const int a = HighlightStore::createCategory("A", CategoryType::Notes);
  const int b = HighlightStore::createCategory("B", CategoryType::Notes);
  EXPECT_EQ(b, a + 1);
  ASSERT_TRUE(HighlightStore::deleteCategory(static_cast<uint16_t>(a)));
  const int c = HighlightStore::createCategory("C", CategoryType::Notes);
  EXPECT_EQ(c, b + 1);
  EXPECT_EQ(HighlightStore::listCategories().size(), 2u);
}

}  // namespace
