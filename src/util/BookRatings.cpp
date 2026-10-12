#include "BookRatings.h"

#include <Epub.h>
#include <FsHelpers.h>
#include <HalStorage.h>
#include <Logging.h>
#include <Xtc.h>

#include <algorithm>

#include "util/BookRatingText.h"

namespace BookRatings {

namespace {

constexpr char kTmpPath[] = "/.crosspoint/ratings.txt.tmp";
constexpr char kHeader[] = "# VARAGH book ratings: stars (0.5 to 5), title, author, file. Separated by tabs.\n";
constexpr char kBookFileName[] = "/rating.txt";
constexpr size_t kMaxLineBytes = 1024;

uint32_t hashOf(const std::string& path) {
  uint32_t hash = 2166136261U;
  for (const char c : path) {
    hash ^= static_cast<uint8_t>(c);
    hash *= 16777619U;
  }
  return hash;
}

// Where the reader keeps its own data for this book; empty for a file that has none.
std::string bookDataPath(const std::string& bookPath) {
  if (FsHelpers::hasEpubExtension(bookPath)) return Epub(bookPath, "/.crosspoint").getCachePath();
  if (FsHelpers::hasXtcExtension(bookPath)) return Xtc(bookPath, "/.crosspoint").getCachePath();
  return {};
}

// Reads the list a line at a time, so its size costs no memory.
class LineReader {
 public:
  explicit LineReader(HalFile& file) : file_(file) {}

  bool next(std::string& line) {
    line.clear();
    bool any = false;
    for (;;) {
      if (at_ == filled_) {
        const int count = file_.read(block_, sizeof(block_));
        if (count <= 0) return any;
        filled_ = static_cast<size_t>(count);
        at_ = 0;
      }
      const char c = static_cast<char>(block_[at_++]);
      any = true;
      if (c == '\n') return true;
      if (line.size() < kMaxLineBytes) line += c;
    }
  }

 private:
  HalFile& file_;
  uint8_t block_[128];
  size_t at_ = 0;
  size_t filled_ = 0;
};

// set() takes the old list away and then gives the new one its name; power
// lost in between leaves only the finished new list under its temporary name.
void recoverList() {
  if (Storage.exists(kListPath) || !Storage.exists(kTmpPath)) return;
  if (!Storage.rename(kTmpPath, kListPath)) Storage.remove(kTmpPath);
}

uint8_t readBookFile(const std::string& bookPath) {
  const std::string dataPath = bookDataPath(bookPath);
  if (dataPath.empty()) return 0;
  const std::string path = dataPath + kBookFileName;
  if (!Storage.exists(path.c_str())) return 0;
  char text[16] = {};
  const size_t length = Storage.readFileToBuffer(path.c_str(), text, sizeof(text) - 1);
  return bookrating::parseNumber(std::string_view(text, length));
}

void writeBookFile(const std::string& bookPath, const uint8_t halfStars) {
  const std::string dataPath = bookDataPath(bookPath);
  if (dataPath.empty()) return;
  const std::string path = dataPath + kBookFileName;
  if (halfStars == 0) {
    if (Storage.exists(path.c_str())) Storage.remove(path.c_str());
    return;
  }
  if (!Storage.exists(dataPath.c_str()) && !Storage.mkdir(dataPath.c_str())) return;
  HalFile file;
  if (!Storage.openFileForWrite("RATE", path, file)) return;
  const std::string text = bookrating::number(halfStars) + "\n";
  file.write(text.data(), text.size());
  file.close();
}

// Writes the list anew: the line of `bookPath` is left out, and `added` (when
// not empty) is put at the end. False leaves the old list as it was.
bool rewriteList(const std::string& bookPath, const std::string& added) {
  Storage.mkdir("/.crosspoint");
  recoverList();
  if (Storage.exists(kTmpPath)) Storage.remove(kTmpPath);
  HalFile out;
  if (!Storage.openFileForWrite("RATE", kTmpPath, out)) return false;
  bool ok = out.write(kHeader, sizeof(kHeader) - 1) == sizeof(kHeader) - 1;

  if (Storage.exists(kListPath)) {
    HalFile in;
    if (!Storage.openFileForRead("RATE", kListPath, in)) {
      out.close();
      Storage.remove(kTmpPath);
      return false;
    }
    LineReader reader(in);
    std::string line;
    while (ok && reader.next(line)) {
      bookrating::ListLine entry;
      // Comments are written anew; a line that says nothing is dropped.
      if (!bookrating::parseLine(line, entry) || entry.path == bookPath) continue;
      line += '\n';
      ok = out.write(line.data(), line.size()) == line.size();
    }
    in.close();
  }
  if (ok && !added.empty()) {
    const std::string line = added + '\n';
    ok = out.write(line.data(), line.size()) == line.size();
  }
  out.close();
  if (!ok) {
    Storage.remove(kTmpPath);
    return false;
  }
  if (Storage.exists(kListPath)) Storage.remove(kListPath);
  return Storage.rename(kTmpPath, kListPath);
}

// The book's line in the list; halfStars 0 when it has none.
struct Found {
  uint8_t halfStars = 0;
  std::string title;
  std::string author;
};

Found findInList(const std::string& bookPath) {
  Found found;
  recoverList();
  if (!Storage.exists(kListPath)) return found;
  HalFile file;
  if (!Storage.openFileForRead("RATE", kListPath, file)) return found;
  LineReader reader(file);
  std::string line;
  while (reader.next(line)) {
    bookrating::ListLine entry;
    if (!bookrating::parseLine(line, entry) || entry.path != bookPath) continue;
    found.halfStars = entry.halfStars;
    found.title.assign(entry.title);
    found.author.assign(entry.author);
    break;
  }
  file.close();
  return found;
}

}  // namespace

uint8_t get(const std::string& bookPath) {
  if (bookPath.empty()) return 0;
  const uint8_t listed = findInList(bookPath).halfStars;
  return listed != 0 ? listed : readBookFile(bookPath);
}

bool set(const std::string& bookPath, const uint8_t halfStars, const std::string& title, const std::string& author) {
  if (bookPath.empty()) return false;
  const uint8_t value = bookrating::clampHalfStars(halfStars);
  // Files and Reading Stats know no author (Files only the file name). Keep the
  // title and author the list already has rather than writing them away.
  Found listedBook;
  if (value != 0 && author.empty()) listedBook = findInList(bookPath);
  const bool keepListed = !listedBook.author.empty();
  const std::string line =
      value == 0 ? std::string()
                 : bookrating::formatLine(value, keepListed ? listedBook.title : title,
                                          keepListed ? listedBook.author : author, bookPath);
  const bool listed = rewriteList(bookPath, line);
  if (!listed) LOG_ERR("RATE", "Could not write %s", kListPath);
  writeBookFile(bookPath, value);
  return listed;
}

void bookMoved(const std::string& oldPath, const std::string& newPath) {
  if (oldPath.empty() || newPath.empty() || oldPath == newPath) return;
  Found found = findInList(oldPath);
  if (found.halfStars == 0) found.halfStars = readBookFile(oldPath);
  if (found.halfStars == 0) return;
  rewriteList(oldPath, std::string());
  set(newPath, found.halfStars, found.title, found.author);
}

void Index::load() {
  entries_.clear();
  recoverList();
  if (!Storage.exists(kListPath)) return;
  HalFile file;
  if (!Storage.openFileForRead("RATE", kListPath, file)) return;
  LineReader reader(file);
  std::string line;
  while (reader.next(line)) {
    bookrating::ListLine entry;
    if (!bookrating::parseLine(line, entry)) continue;
    entries_.push_back(Entry{hashOf(std::string(entry.path)), entry.halfStars});
  }
  file.close();
  std::sort(entries_.begin(), entries_.end(), [](const Entry& a, const Entry& b) { return a.pathHash < b.pathHash; });
}

uint8_t Index::find(const std::string& bookPath) const {
  if (entries_.empty()) return 0;
  const uint32_t hash = hashOf(bookPath);
  const auto it = std::lower_bound(entries_.begin(), entries_.end(), hash,
                                   [](const Entry& entry, const uint32_t value) { return entry.pathHash < value; });
  return it != entries_.end() && it->pathHash == hash ? it->halfStars : 0;
}

}  // namespace BookRatings
