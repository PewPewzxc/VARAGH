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
constexpr size_t kMaxSavedWords = 4000;
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

// Reads the saved words of a language. False only when an existing index
// cannot be read; a language with nothing saved yet is an empty list.
bool readIndex(const std::string& idxPath, std::vector<SavedWord>& words) {
  words.clear();
  if (!Storage.exists(idxPath.c_str())) return true;
  HalFile file;
  if (!Storage.openFileForRead("ODICT", idxPath, file)) return false;
  std::string word;
  while (words.size() < kMaxSavedWords) {
    word.clear();
    int c = file.read();
    if (c < 0) break;  // clean end of file
    while (c > 0 && word.size() < kMaxWordBytes) {
      word += static_cast<char>(c);
      c = file.read();
    }
    uint8_t tail[8];
    if (c != 0 || file.read(tail, sizeof(tail)) != static_cast<int>(sizeof(tail))) {
      LOG_ERR("ODICT", "Saved-answers index is cut short: %s", idxPath.c_str());
      file.close();
      return false;
    }
    words.push_back(SavedWord{word, readBigEndian(tail), readBigEndian(tail + 4)});
  }
  file.close();
  return true;
}

bool writeWholeFile(const std::string& path, const std::string& content) {
  HalFile file;
  if (!Storage.openFileForWrite("ODICT", path, file)) return false;
  const bool ok = file.write(content.data(), content.size()) == content.size();
  file.close();
  return ok;
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
  std::vector<SavedWord> words;
  if (!readIndex(DictPaths(storePath(lang)).idx(), words)) return false;
  return std::any_of(words.begin(), words.end(), [&](const SavedWord& saved) { return sameWord(saved.word, word); });
}

bool save(const onlinedict::Lang lang, const std::string& word, const std::string& html) {
  if (word.empty() || word.size() > kMaxWordBytes || html.empty()) return false;
  Storage.mkdir(kStoreDir);
  const std::string base = storePath(lang);
  const DictPaths paths(base);

  std::vector<SavedWord> words;
  if (!readIndex(paths.idx(), words)) return false;

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

  const auto existing =
      std::find_if(words.begin(), words.end(), [&](const SavedWord& saved) { return saved.word == word; });
  if (existing != words.end()) {
    existing->offset = offset;
    existing->size = static_cast<uint32_t>(html.size());
  } else {
    words.push_back(SavedWord{word, offset, static_cast<uint32_t>(html.size())});
  }
  std::sort(words.begin(), words.end(),
            [](const SavedWord& a, const SavedWord& b) { return compareWords(a.word, b.word) < 0; });

  std::string index;
  for (const SavedWord& saved : words) {
    index += saved.word;
    index += '\0';
    uint8_t tail[8];
    writeBigEndian(tail, saved.offset);
    writeBigEndian(tail + 4, saved.size);
    index.append(reinterpret_cast<const char*>(tail), sizeof(tail));
  }
  // Write beside the old index and swap, so a failed write keeps the old answers reachable.
  const std::string tmpPath = paths.idx() + ".tmp";
  if (!writeWholeFile(tmpPath, index)) {
    Storage.remove(tmpPath.c_str());
    return false;
  }
  if (Storage.exists(paths.idx().c_str())) Storage.remove(paths.idx().c_str());
  if (!Storage.rename(tmpPath.c_str(), paths.idx().c_str())) return false;

  // A lookup aid built for the previous index would point at the wrong words.
  for (const std::string& stale : {paths.quickIdx(), paths.idxOft(), paths.idxOftCspt()}) {
    if (Storage.exists(stale.c_str())) Storage.remove(stale.c_str());
  }

  char info[320];
  snprintf(info, sizeof(info),
           "StarDict's dict ifo file\nversion=2.4.2\nwordcount=%u\nidxfilesize=%u\n"
           "bookname=Online (Wiktionary)\nsametypesequence=h\nlang=%s-%s\n"
           "description=Answers saved by the VARAGH online dictionary. Wiktionary and Wikipedia, CC BY-SA 4.0.\n",
           static_cast<unsigned>(words.size()), static_cast<unsigned>(index.size()), onlinedict::code(lang),
           onlinedict::code(lang));
  return writeWholeFile(paths.ifo(), info);
}

Outcome fetch(const onlinedict::Lang lang, std::string& word, std::string& html) {
  // 1 = entry found, 0 = the site answered without one, -1 = no answer.
  // Wikimedia answers "no such page" on its REST addresses with an error
  // status, which the downloader reports the same way as a dead connection.
  const auto request = [&](const std::string& url, const onlinedict::Source source, const std::string& title) {
    onlinedict::Extractor extractor(source, lang, title);
    size_t received = 0;
    const HttpDownloader::DataCallback onData = [&](const uint8_t* data, const size_t length) {
      received += length;
      extractor.feed(reinterpret_cast<const char*>(data), length);
      return received <= kMaxAnswerBytes;
    };
    if (!HttpDownloader::fetchUrl(url, onData)) return -1;
    return extractor.finish(html) ? 1 : 0;
  };

  const std::string asked = onlinedict::normalizeWord(word);
  if (asked.empty()) return Outcome::NotFound;
  const std::string otherCase = onlinedict::otherCase(asked);
  const onlinedict::Source dictionarySource = lang == onlinedict::Lang::English
                                                  ? onlinedict::Source::WiktionaryDefinitions
                                                  : onlinedict::Source::WiktionaryExtract;
  bool siteAnswered = false;
  for (const std::string& title : {asked, otherCase}) {
    if (title.empty()) continue;
    const int result = request(onlinedict::definitionUrl(lang, title), dictionarySource, title);
    if (result == 1) {
      word = title;
      return Outcome::Found;
    }
    siteAnswered = siteAnswered || result == 0;
  }

  // Names, places and ideas are Wikipedia's ground.
  const int summary = request(onlinedict::summaryUrl(lang, asked), onlinedict::Source::WikipediaSummary, asked);
  if (summary == 1) {
    word = asked;
    return Outcome::Found;
  }
  siteAnswered = siteAnswered || summary == 0;

  if (!siteAnswered) {
    const HttpDownloader::DataCallback discard = [](const uint8_t*, size_t) { return true; };
    siteAnswered = HttpDownloader::fetchUrl(onlinedict::probeUrl(lang), discard);
  }
  LOG_INF("ODICT", "No online entry (site reached: %d)", siteAnswered ? 1 : 0);
  return siteAnswered ? Outcome::NotFound : Outcome::NoConnection;
}

}  // namespace OnlineDictionary
