#include "OnlineDictionary.h"

#include <HalStorage.h>
#include <Logging.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <vector>

#include "network/HttpDownloader.h"
#include "util/Dictionary.h"

namespace OnlineDictionary {

namespace {

constexpr char kRequestPath[] = "/.crosspoint/online-lookup.txt";
constexpr char kStoreDir[] = "/.crosspoint/online-dict";
// An answer larger than this is not a dictionary entry worth waiting for.
constexpr size_t kMaxAnswerBytes = 600U * 1024U;
constexpr size_t kMaxWordBytes = 200;

struct SavedWord {
  std::string word;
  uint32_t offset = 0;
  uint32_t size = 0;
};

// The order the dictionary reader expects of an .idx: letters compared without
// regard to ASCII case, bytes deciding between words that differ only in case.
int compareWords(const std::string& a, const std::string& b) {
  const size_t shared = std::min(a.size(), b.size());
  for (size_t i = 0; i < shared; ++i) {
    const int diff = std::tolower(static_cast<unsigned char>(a[i])) - std::tolower(static_cast<unsigned char>(b[i]));
    if (diff != 0) return diff;
  }
  if (a.size() != b.size()) return a.size() < b.size() ? -1 : 1;
  return a.compare(b);
}

bool sameWord(const std::string& a, const std::string& b) {
  if (a.size() != b.size()) return false;
  for (size_t i = 0; i < a.size(); ++i) {
    if (std::tolower(static_cast<unsigned char>(a[i])) != std::tolower(static_cast<unsigned char>(b[i]))) return false;
  }
  return true;
}

uint32_t readBigEndian(const uint8_t* bytes) {
  return (static_cast<uint32_t>(bytes[0]) << 24) | (static_cast<uint32_t>(bytes[1]) << 16) |
         (static_cast<uint32_t>(bytes[2]) << 8) | bytes[3];
}

void writeBigEndian(uint8_t* bytes, const uint32_t value) {
  bytes[0] = static_cast<uint8_t>(value >> 24);
  bytes[1] = static_cast<uint8_t>(value >> 16);
  bytes[2] = static_cast<uint8_t>(value >> 8);
  bytes[3] = static_cast<uint8_t>(value);
}

// Reads the index of saved words an entry at a time (a word, a zero byte, then
// its offset and size), so that any number of saved words costs the same small
// amount of memory.
class IndexReader {
 public:
  explicit IndexReader(HalFile& file) : file_(file) {}

  // False at the end of the file; cutShort() then tells a clean end from one
  // in the middle of an entry.
  bool next(SavedWord& entry) {
    entry.word.clear();
    int c = get();
    if (c < 0) return false;
    while (c > 0 && entry.word.size() < kMaxWordBytes) {
      entry.word += static_cast<char>(c);
      c = get();
    }
    uint8_t tail[8];
    for (size_t i = 0; c == 0 && i < sizeof(tail); ++i) {
      const int byte = get();
      if (byte < 0) {
        c = -1;
      } else {
        tail[i] = static_cast<uint8_t>(byte);
      }
    }
    if (c != 0) {
      cutShort_ = true;
      return false;
    }
    entry.offset = readBigEndian(tail);
    entry.size = readBigEndian(tail + 4);
    return true;
  }
  bool cutShort() const { return cutShort_; }

 private:
  int get() {
    if (at_ == filled_) {
      const int count = file_.read(block_, sizeof(block_));
      if (count <= 0) return -1;
      filled_ = static_cast<size_t>(count);
      at_ = 0;
    }
    return block_[at_++];
  }

  HalFile& file_;
  uint8_t block_[128];
  size_t at_ = 0;
  size_t filled_ = 0;
  bool cutShort_ = false;
};

void appendEntry(std::string& out, const SavedWord& entry) {
  out += entry.word;
  out += '\0';
  uint8_t tail[8];
  writeBigEndian(tail, entry.offset);
  writeBigEndian(tail + 4, entry.size);
  out.append(reinterpret_cast<const char*>(tail), sizeof(tail));
}

bool writeWholeFile(const std::string& path, const std::string& content) {
  HalFile file;
  if (!Storage.openFileForWrite("ODICT", path, file)) return false;
  const bool ok = file.write(content.data(), content.size()) == content.size();
  file.close();
  return ok;
}

// Counts the entries of an index and its bytes. False when it cannot be read
// or ends in the middle of an entry.
bool measureIndex(const std::string& idxPath, uint32_t& wordCount, uint32_t& indexBytes) {
  wordCount = 0;
  indexBytes = 0;
  HalFile file;
  if (!Storage.openFileForRead("ODICT", idxPath, file)) return false;
  IndexReader reader(file);
  SavedWord entry;
  while (reader.next(entry)) {
    ++wordCount;
    indexBytes += static_cast<uint32_t>(entry.word.size() + 9);
  }
  file.close();
  return !reader.cutShort();
}

// Writes the old index to newPath with `added` in its place in the order, in
// place of an entry for exactly the same word if there is one. The old index is
// read and the new one written an entry at a time.
bool mergeIndex(const std::string& oldPath, const std::string& newPath, const SavedWord& added, uint32_t& wordCount,
                uint32_t& indexBytes) {
  wordCount = 0;
  indexBytes = 0;
  HalFile out;
  if (!Storage.openFileForWrite("ODICT", newPath, out)) return false;
  std::string pending;
  pending.reserve(768);
  bool ok = true;
  const auto write = [&](const SavedWord& entry) {
    appendEntry(pending, entry);
    ++wordCount;
    indexBytes += static_cast<uint32_t>(entry.word.size() + 9);
    if (pending.size() >= 512) {
      ok = ok && out.write(pending.data(), pending.size()) == pending.size();
      pending.clear();
    }
  };

  bool placed = false;
  if (Storage.exists(oldPath.c_str())) {
    HalFile in;
    if (!Storage.openFileForRead("ODICT", oldPath, in)) {
      out.close();
      return false;
    }
    IndexReader reader(in);
    SavedWord entry;
    while (ok && reader.next(entry)) {
      if (!placed) {
        const int order = compareWords(added.word, entry.word);
        if (order <= 0) {
          write(added);
          placed = true;
          if (order == 0) continue;  // the same word: its old entry goes
        }
      }
      write(entry);
    }
    in.close();
    // A damaged index keeps the words that could still be read rather than
    // refusing every new answer from now on.
    if (reader.cutShort()) LOG_ERR("ODICT", "Saved-answers index is cut short, the rest is dropped: %s", oldPath.c_str());
  }
  if (!placed) write(added);
  if (ok && !pending.empty()) ok = out.write(pending.data(), pending.size()) == pending.size();
  out.close();
  return ok;
}

bool writeInfo(const DictPaths& paths, const onlinedict::Lang lang, const uint32_t wordCount,
               const uint32_t indexBytes) {
  char info[320];
  snprintf(info, sizeof(info),
           "StarDict's dict ifo file\nversion=2.4.2\nwordcount=%u\nidxfilesize=%u\n"
           "bookname=Online (Wiktionary)\nsametypesequence=h\nlang=%s-%s\n"
           "description=Answers saved by the VARAGH online dictionary. Wiktionary and Wikipedia, CC BY-SA 4.0.\n",
           static_cast<unsigned>(wordCount), static_cast<unsigned>(indexBytes), onlinedict::code(lang),
           onlinedict::code(lang));
  return writeWholeFile(paths.ifo(), info);
}

// A lookup aid built for an earlier index would point at the wrong words.
void removeLookupAids(const DictPaths& paths) {
  for (const std::string& stale : {paths.quickIdx(), paths.idxOft(), paths.idxOftCspt()}) {
    if (Storage.exists(stale.c_str())) Storage.remove(stale.c_str());
  }
}

std::string temporaryIndexPath(const DictPaths& paths) { return paths.idx() + ".tmp"; }

// save() takes the old index away and then gives the new one its name. Power
// lost between the two leaves only the finished new index, under its temporary
// name: it is put in place here. (Power lost earlier leaves the old index and
// an unfinished temporary one, which the next save simply writes again.)
void recoverIndex(const DictPaths& paths, const onlinedict::Lang lang) {
  const std::string tmpPath = temporaryIndexPath(paths);
  if (Storage.exists(paths.idx().c_str()) || !Storage.exists(tmpPath.c_str())) return;
  uint32_t wordCount = 0;
  uint32_t indexBytes = 0;
  // The very first save has no old index to protect it: only a whole one counts.
  if (!measureIndex(tmpPath, wordCount, indexBytes) || wordCount == 0 ||
      !Storage.rename(tmpPath.c_str(), paths.idx().c_str())) {
    Storage.remove(tmpPath.c_str());
    return;
  }
  LOG_INF("ODICT", "Put back a saved-answers index of %u words", static_cast<unsigned>(wordCount));
  removeLookupAids(paths);
  writeInfo(paths, lang, wordCount, indexBytes);
}

}  // namespace

bool saveRequest(const Request& request) {
  Storage.mkdir("/.crosspoint");
  std::string content = request.answered ? "answered\n" : "ask\n";
  content += onlinedict::code(request.lang);
  content += '\n';
  content += request.word;
  content += '\n';
  return writeWholeFile(kRequestPath, content);
}

bool loadRequest(Request& request) {
  if (!Storage.exists(kRequestPath)) return false;
  char buffer[kMaxWordBytes + 32];
  const size_t length = Storage.readFileToBuffer(kRequestPath, buffer, sizeof(buffer));
  const std::string content(buffer, length);
  const size_t firstBreak = content.find('\n');
  const size_t secondBreak = firstBreak == std::string::npos ? firstBreak : content.find('\n', firstBreak + 1);
  if (secondBreak == std::string::npos) return false;
  const std::string status = content.substr(0, firstBreak);
  const std::string langCode = content.substr(firstBreak + 1, secondBreak - firstBreak - 1);
  std::string word = content.substr(secondBreak + 1);
  while (!word.empty() && (word.back() == '\n' || word.back() == '\r')) word.pop_back();
  if (word.empty() || !onlinedict::langFromCode(langCode.c_str(), request.lang)) return false;
  request.word = word;
  request.answered = status == "answered";
  return true;
}

void clearRequest() {
  if (Storage.exists(kRequestPath)) Storage.remove(kRequestPath);
}

std::string storePath(const onlinedict::Lang lang) {
  return std::string(kStoreDir) + "/online-" + onlinedict::code(lang);
}

bool hasSaved(const onlinedict::Lang lang, const std::string& word) {
  const std::string base = storePath(lang);
  const DictPaths paths(base);
  recoverIndex(paths, lang);
  if (!Storage.exists(paths.idx().c_str())) return false;
  HalFile file;
  if (!Storage.openFileForRead("ODICT", paths.idx(), file)) return false;
  IndexReader reader(file);
  SavedWord entry;
  bool found = false;
  while (!found && reader.next(entry)) found = sameWord(entry.word, word);
  file.close();
  return found;
}

bool save(const onlinedict::Lang lang, const std::string& word, const std::string& html) {
  if (word.empty() || word.size() > kMaxWordBytes || html.empty()) return false;
  Storage.mkdir(kStoreDir);
  const std::string base = storePath(lang);
  const DictPaths paths(base);
  recoverIndex(paths, lang);

  // Definitions are only ever appended; a replaced answer leaves its old bytes behind.
  HalFile dict = Storage.open(paths.dict().c_str(), O_WRONLY | O_CREAT | O_AT_END);
  if (!dict) {
    LOG_ERR("ODICT", "Cannot open %s", paths.dict().c_str());
    return false;
  }
  const auto offset = static_cast<uint32_t>(dict.fileSize());
  const bool written = dict.write(html.data(), html.size()) == html.size();
  dict.close();
  if (!written) return false;

  // The new index is written beside the old one and then takes its place, so a
  // failed write keeps the old answers reachable.
  const std::string tmpPath = temporaryIndexPath(paths);
  if (Storage.exists(tmpPath.c_str())) Storage.remove(tmpPath.c_str());
  uint32_t wordCount = 0;
  uint32_t indexBytes = 0;
  if (!mergeIndex(paths.idx(), tmpPath, SavedWord{word, offset, static_cast<uint32_t>(html.size())}, wordCount,
                  indexBytes)) {
    Storage.remove(tmpPath.c_str());
    return false;
  }
  if (Storage.exists(paths.idx().c_str())) Storage.remove(paths.idx().c_str());
  if (!Storage.rename(tmpPath.c_str(), paths.idx().c_str())) return false;

  removeLookupAids(paths);
  return writeInfo(paths, lang, wordCount, indexBytes);
}

Outcome fetch(const onlinedict::Lang lang, std::string& word, std::string& html, const FetchOptions& options) {
  enum class Answer : uint8_t { Entry, NoEntry, AskAgain, Unreachable, Cancelled };
  // found gets the spelling an entry was found under.
  const auto request = [&](const std::string& url, const onlinedict::Source source, const std::string& title,
                           const std::string& otherSpelling, std::string& found) {
    onlinedict::Extractor extractor(source, lang, title, otherSpelling);
    size_t received = 0;
    const HttpDownloader::DataCallback onData = [&](const uint8_t* data, const size_t length) {
      received += length;
      extractor.feed(reinterpret_cast<const char*>(data), length);
      return received <= kMaxAnswerBytes;
    };
    int status = 0;
    HttpDownloader::DownloadOptions download;
    download.shouldCancel = options.cancelled;
    download.timeoutMs = options.timeoutMs;
    download.httpStatus = &status;
    const HttpDownloader::DownloadError error =
        HttpDownloader::streamUrl(url, onData, nullptr, "", "", std::move(download));
    if (error == HttpDownloader::ABORTED) return Answer::Cancelled;
    if (error == HttpDownloader::OK) {
      if (extractor.finish(html)) {
        found = extractor.foundWord();
        return Answer::Entry;
      }
      return extractor.askAgain() ? Answer::AskAgain : Answer::NoEntry;
    }
    // The site did answer: "no such page" (how its REST addresses say that),
    // or more than an entry can be.
    if (status == 404 || (status == 200 && received > kMaxAnswerBytes)) return Answer::NoEntry;
    // No answer at all, or one that says nothing about the word. Asking the
    // remaining questions would only make the reader wait as long again.
    return Answer::Unreachable;
  };

  const std::string asked = onlinedict::normalizeWord(word);
  if (asked.empty()) return Outcome::NotFound;
  const std::string otherCase = onlinedict::otherCase(asked);
  const onlinedict::Source dictionarySource = lang == onlinedict::Lang::English
                                                  ? onlinedict::Source::WiktionaryDefinitions
                                                  : onlinedict::Source::WiktionaryExtract;
  // A German word with a capital first letter (a noun, or any word that starts
  // a sentence) is asked for together with its lower-case spelling. The site
  // sends the capitalised page first, so the word itself still wins and one
  // request does for both. The other way round the capitalised page would
  // crowd out the word that was asked for, so those stay two requests.
  const bool together =
      lang != onlinedict::Lang::English && !otherCase.empty() && asked[0] >= 'A' && asked[0] <= 'Z';

  std::string found;
  bool otherAnswered = false;
  const std::string* const titles[2] = {&asked, &otherCase};
  for (int i = 0; i < 2; ++i) {
    const std::string& title = *titles[i];
    if (title.empty() || (i == 1 && otherAnswered)) continue;
    const bool both = i == 0 && together;
    Answer answer = request(onlinedict::definitionUrl(lang, title, both ? otherCase : std::string()), dictionarySource,
                            title, both ? otherCase : std::string(), found);
    const bool askedAgain = answer == Answer::AskAgain;
    if (askedAgain) {
      answer = request(onlinedict::definitionUrl(lang, title), dictionarySource, title, std::string(), found);
      if (answer == Answer::AskAgain) answer = Answer::NoEntry;
    }
    if (answer == Answer::Entry) {
      word = found;
      return Outcome::Found;
    }
    if (answer == Answer::Cancelled) return Outcome::Cancelled;
    if (answer == Answer::Unreachable) return Outcome::NoConnection;
    otherAnswered = both && !askedAgain;
  }

  // Names, places and ideas are Wikipedia's ground.
  const Answer summary =
      request(onlinedict::summaryUrl(lang, asked), onlinedict::Source::WikipediaSummary, asked, std::string(), found);
  if (summary == Answer::Entry) {
    word = asked;
    return Outcome::Found;
  }
  if (summary == Answer::Cancelled) return Outcome::Cancelled;
  if (summary == Answer::Unreachable) return Outcome::NoConnection;
  LOG_INF("ODICT", "No online entry");
  return Outcome::NotFound;
}

}  // namespace OnlineDictionary
