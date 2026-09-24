#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "HighlightFormat.h"

// User highlight categories ("Favorite Lines", "Flashcards", and any the user
// creates), stored on the SD card:
//
//   /.crosspoint/highlights/c<id>.txt   header line + one entry per line
//   /.crosspoint/highlights/c<id>.lvl   flashcard box per entry, 1 byte each
//
// Entries are appended, so adding one never rewrites the file. Lists read
// only a line-offset index (4 bytes per entry) and then the rows on screen,
// so RAM stays flat however large a category grows. Flashcard answers touch
// a single byte of the .lvl file.
namespace HighlightStore {

struct CategoryInfo {
  uint16_t id = 0;
  HighlightFormat::CategoryType type = HighlightFormat::CategoryType::Notes;
  std::string name;
  uint16_t count = 0;
};

constexpr size_t MAX_CATEGORIES = 32;
constexpr size_t MAX_ENTRY_BYTES = 4096;    // longest line read back
constexpr size_t MAX_MEANING_BYTES = 1500;  // saved flashcard meaning

// Create "Favorite Lines" and "Flashcards" the first time the store is used.
void ensureDefaults();

std::vector<CategoryInfo> listCategories();
bool findCategory(uint16_t id, CategoryInfo& out);

// Returns the new id, or -1.
int createCategory(const std::string& name, HighlightFormat::CategoryType type);
bool renameCategory(uint16_t id, const std::string& name);
bool deleteCategory(uint16_t id);

bool addEntry(uint16_t id, const HighlightFormat::Entry& entry);
// File offset of every entry line, in order.
bool loadEntryOffsets(uint16_t id, std::vector<uint32_t>& offsets);
bool readEntry(uint16_t id, uint32_t offset, HighlightFormat::Entry& out);
// Read entries [first, first + count) of `offsets` with a single file open
// (a screenful of list rows). Unreadable lines come back empty.
bool readEntries(uint16_t id, const std::vector<uint32_t>& offsets, size_t first, size_t count,
                 std::vector<HighlightFormat::Entry>& out);
bool deleteEntry(uint16_t id, size_t index);

// Flashcard boxes (0 = new). Missing bytes read as 0.
bool loadLevels(uint16_t id, size_t count, std::vector<uint8_t>& levels);
bool setLevel(uint16_t id, size_t index, uint8_t level);

}  // namespace HighlightStore
