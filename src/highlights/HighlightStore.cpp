#include "HighlightStore.h"

#include <HalStorage.h>
#include <I18n.h>
#include <Logging.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace HighlightStore {
namespace {

constexpr const char* DIR = "/.crosspoint/highlights";
constexpr const char* INIT_MARKER = "/.crosspoint/highlights/.init";

void textPath(const uint16_t id, char* out, const size_t size) { snprintf(out, size, "%s/c%u.txt", DIR, id); }
void levelPath(const uint16_t id, char* out, const size_t size) { snprintf(out, size, "%s/c%u.lvl", DIR, id); }
void tempPath(char* out, const size_t size) { snprintf(out, size, "%s/tmp.part", DIR); }

// Buffered line reader: SD reads are far cheaper in chunks than per byte.
class LineReader {
 public:
  explicit LineReader(HalFile& file) : file_(file) {}

  // Reads the next line without its newline. Lines longer than maxLen are
  // truncated (the rest is skipped). Returns false at end of file.
  bool next(std::string& line, const size_t maxLen, uint32_t* startOffset = nullptr) {
    line.clear();
    if (startOffset) *startOffset = offset_;
    bool any = false;
    while (true) {
      if (pos_ >= len_) {
        const int n = file_.read(buf_, sizeof(buf_));
        if (n <= 0) return any;
        len_ = static_cast<size_t>(n);
        pos_ = 0;
      }
      any = true;
      const char c = buf_[pos_++];
      ++offset_;
      if (c == '\n') return true;
      if (line.size() < maxLen) line += c;
    }
  }

 private:
  HalFile& file_;
  char buf_[256];
  size_t pos_ = 0;
  size_t len_ = 0;
  uint32_t offset_ = 0;
};

bool writeAll(HalFile& file, const std::string& text) {
  return file.write(text.data(), text.size()) == text.size();
}

bool readHeader(const uint16_t id, CategoryInfo& info, const bool countEntries) {
  char path[64];
  textPath(id, path, sizeof(path));
  HalFile file;
  if (!Storage.openFileForRead("HLS", path, file)) return false;
  LineReader reader(file);
  std::string line;
  if (!reader.next(line, 512) || !HighlightFormat::parseHeader(line, info.type, info.name)) {
    file.close();
    return false;
  }
  info.id = id;
  info.count = 0;
  if (countEntries) {
    while (reader.next(line, 0)) {
      if (info.count < UINT16_MAX) ++info.count;
    }
  }
  file.close();
  return true;
}

bool parseCategoryFileName(const char* name, uint16_t& id) {
  if (name[0] != 'c') return false;
  unsigned value = 0;
  const char* p = name + 1;
  if (*p < '0' || *p > '9') return false;
  while (*p >= '0' && *p <= '9') value = value * 10 + static_cast<unsigned>(*p++ - '0');
  if (std::strcmp(p, ".txt") != 0 || value == 0 || value > UINT16_MAX) return false;
  id = static_cast<uint16_t>(value);
  return true;
}

std::vector<uint16_t> categoryIds() {
  std::vector<uint16_t> ids;
  HalFile dir = Storage.open(DIR);
  if (!dir || !dir.isDirectory()) return ids;
  char name[64];
  while (true) {
    HalFile entry = dir.openNextFile();
    if (!entry) break;
    const bool isDir = entry.isDirectory();
    if (!isDir) entry.getName(name, sizeof(name));
    entry.close();
    uint16_t id = 0;
    if (!isDir && parseCategoryFileName(name, id) && ids.size() < MAX_CATEGORIES) ids.push_back(id);
  }
  dir.close();
  std::sort(ids.begin(), ids.end());
  return ids;
}

// Copy `src` to a temp file, letting `keep` drop lines, then replace `src`.
// Line 0 is the header. Used for the rare rewrite paths (rename, delete).
template <typename KeepFn>
bool rewriteLines(const char* src, KeepFn&& keep, const std::string* replacementHeader) {
  char tmp[64];
  tempPath(tmp, sizeof(tmp));
  HalFile in;
  if (!Storage.openFileForRead("HLS", src, in)) return false;
  HalFile out;
  if (!Storage.openFileForWrite("HLS", tmp, out)) {
    in.close();
    return false;
  }
  LineReader reader(in);
  std::string line;
  size_t lineNo = 0;
  bool ok = true;
  while (ok && reader.next(line, MAX_ENTRY_BYTES)) {
    if (lineNo == 0 && replacementHeader) line = *replacementHeader;
    if (lineNo == 0 || keep(lineNo - 1)) ok = writeAll(out, line) && out.write('\n') == 1;
    ++lineNo;
  }
  in.close();
  ok = out.close() && ok;
  if (!ok) {
    Storage.remove(tmp);
    LOG_ERR("HLS", "Rewrite of %s failed", src);
    return false;
  }
  Storage.remove(src);
  return Storage.rename(tmp, src);
}

}  // namespace

void ensureDefaults() {
  if (Storage.exists(INIT_MARKER)) return;
  Storage.ensureDirectoryExists(DIR);
  if (categoryIds().empty()) {
    createCategory(tr(STR_FAVORITE_LINES), HighlightFormat::CategoryType::Notes);
    createCategory(tr(STR_FLASHCARDS), HighlightFormat::CategoryType::Flashcards);
  }
  HalFile marker;
  if (Storage.openFileForWrite("HLS", INIT_MARKER, marker)) marker.close();
}

std::vector<CategoryInfo> listCategories() {
  std::vector<CategoryInfo> out;
  const auto ids = categoryIds();
  out.reserve(ids.size());
  for (const uint16_t id : ids) {
    CategoryInfo info;
    if (readHeader(id, info, true)) out.push_back(std::move(info));
  }
  return out;
}

bool findCategory(const uint16_t id, CategoryInfo& out) { return readHeader(id, out, true); }

int createCategory(const std::string& name, const HighlightFormat::CategoryType type) {
  Storage.ensureDirectoryExists(DIR);
  const auto ids = categoryIds();
  if (ids.size() >= MAX_CATEGORIES) return -1;
  const uint16_t id = ids.empty() ? 1 : static_cast<uint16_t>(ids.back() + 1);
  char path[64];
  textPath(id, path, sizeof(path));
  HalFile file;
  if (!Storage.openFileForWrite("HLS", path, file)) return -1;
  const bool ok = writeAll(file, HighlightFormat::headerLine(type, name)) && file.write('\n') == 1;
  file.close();
  if (!ok) {
    Storage.remove(path);
    return -1;
  }
  return id;
}

bool renameCategory(const uint16_t id, const std::string& name) {
  CategoryInfo info;
  if (!readHeader(id, info, false)) return false;
  char path[64];
  textPath(id, path, sizeof(path));
  const std::string header = HighlightFormat::headerLine(info.type, name);
  return rewriteLines(path, [](size_t) { return true; }, &header);
}

bool deleteCategory(const uint16_t id) {
  char path[64];
  levelPath(id, path, sizeof(path));
  if (Storage.exists(path)) Storage.remove(path);
  textPath(id, path, sizeof(path));
  return Storage.remove(path);
}

bool addEntry(const uint16_t id, const HighlightFormat::Entry& entry) {
  char path[64];
  textPath(id, path, sizeof(path));
  if (!Storage.exists(path)) return false;
  HalFile file = Storage.open(path, O_WRONLY | O_APPEND);
  if (!file) {
    LOG_ERR("HLS", "Cannot append to %s", path);
    return false;
  }
  const bool ok = writeAll(file, HighlightFormat::entryLine(entry)) && file.write('\n') == 1;
  file.close();
  return ok;
}

bool loadEntryOffsets(const uint16_t id, std::vector<uint32_t>& offsets) {
  offsets.clear();
  char path[64];
  textPath(id, path, sizeof(path));
  HalFile file;
  if (!Storage.openFileForRead("HLS", path, file)) return false;
  LineReader reader(file);
  std::string line;
  uint32_t start = 0;
  reader.next(line, 0);  // header
  // Every line counts (like deleteEntry's rewrite), so indexes stay aligned.
  while (reader.next(line, 0, &start)) offsets.push_back(start);
  file.close();
  return true;
}

bool readEntry(const uint16_t id, const uint32_t offset, HighlightFormat::Entry& out) {
  char path[64];
  textPath(id, path, sizeof(path));
  HalFile file;
  if (!Storage.openFileForRead("HLS", path, file)) return false;
  bool ok = file.seekSet(offset);
  if (ok) {
    LineReader reader(file);
    std::string line;
    ok = reader.next(line, MAX_ENTRY_BYTES) && HighlightFormat::parseEntry(line, out);
  }
  file.close();
  return ok;
}

bool readEntries(const uint16_t id, const std::vector<uint32_t>& offsets, const size_t first, const size_t count,
                 std::vector<HighlightFormat::Entry>& out) {
  out.clear();
  if (first >= offsets.size()) return true;
  const size_t end = std::min(offsets.size(), first + count);
  out.resize(end - first);
  char path[64];
  textPath(id, path, sizeof(path));
  HalFile file;
  if (!Storage.openFileForRead("HLS", path, file)) return false;
  std::string line;
  for (size_t i = first; i < end; ++i) {
    if (!file.seekSet(offsets[i])) continue;
    LineReader reader(file);
    if (reader.next(line, MAX_ENTRY_BYTES)) HighlightFormat::parseEntry(line, out[i - first]);
  }
  file.close();
  return true;
}

bool deleteEntry(const uint16_t id, const size_t index) {
  char path[64];
  textPath(id, path, sizeof(path));
  if (!rewriteLines(path, [index](const size_t entryIndex) { return entryIndex != index; }, nullptr)) return false;

  // Keep the level file aligned: drop the same byte.
  char lvl[64];
  levelPath(id, lvl, sizeof(lvl));
  if (!Storage.exists(lvl)) return true;
  std::vector<uint8_t> levels;
  HalFile in;
  if (Storage.openFileForRead("HLS", lvl, in)) {
    levels.resize(in.fileSize());
    const int n = levels.empty() ? 0 : in.read(levels.data(), levels.size());
    in.close();
    levels.resize(n > 0 ? static_cast<size_t>(n) : 0);
  }
  if (index < levels.size()) levels.erase(levels.begin() + static_cast<long>(index));
  HalFile out;
  if (!Storage.openFileForWrite("HLS", lvl, out)) return false;
  const bool ok = levels.empty() || out.write(levels.data(), levels.size()) == levels.size();
  out.close();
  return ok;
}

bool loadLevels(const uint16_t id, const size_t count, std::vector<uint8_t>& levels) {
  levels.assign(count, 0);
  char path[64];
  levelPath(id, path, sizeof(path));
  HalFile file;
  if (!Storage.openFileForRead("HLS", path, file)) return true;  // no answers yet
  const size_t n = std::min(count, static_cast<size_t>(file.fileSize()));
  if (n > 0) file.read(levels.data(), n);
  file.close();
  return true;
}

bool setLevel(const uint16_t id, const size_t index, const uint8_t level) {
  char path[64];
  levelPath(id, path, sizeof(path));
  HalFile file = Storage.open(path, O_RDWR | O_CREAT);
  if (!file) return false;
  const size_t size = file.fileSize();
  bool ok = true;
  if (size < index) {
    // Pad unanswered cards with 0 so byte `index` lands at its entry.
    ok = file.seekSet(size);
    static constexpr uint8_t zeros[32] = {};
    for (size_t left = index - size; ok && left > 0;) {
      const size_t chunk = std::min(left, sizeof(zeros));
      ok = file.write(zeros, chunk) == chunk;
      left -= chunk;
    }
  }
  ok = ok && file.seekSet(index) && file.write(&level, 1) == 1;
  file.close();
  return ok;
}

}  // namespace HighlightStore
