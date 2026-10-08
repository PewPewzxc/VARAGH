#include "OnlineDictionaryText.h"

#include <cstdlib>
#include <cstring>
#include <utility>
#include <vector>

namespace onlinedict {

namespace {

constexpr size_t kMaxEntryBytes = 4000;
constexpr size_t kMaxSenseBytes = 320;
constexpr size_t kMaxExampleBytes = 220;
constexpr size_t kMaxSummaryBytes = 900;
constexpr int kMaxSections = 3;
constexpr int kFirstSectionSenses = 4;
constexpr int kLaterSectionSenses = 2;
constexpr int kMaxExamples = 2;

void appendUtf8(std::string& out, const uint32_t cp) {
  if (cp < 0x80) {
    out += static_cast<char>(cp);
  } else if (cp < 0x800) {
    out += static_cast<char>(0xC0 | (cp >> 6));
    out += static_cast<char>(0x80 | (cp & 0x3F));
  } else if (cp < 0x10000) {
    out += static_cast<char>(0xE0 | (cp >> 12));
    out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
    out += static_cast<char>(0x80 | (cp & 0x3F));
  } else {
    out += static_cast<char>(0xF0 | (cp >> 18));
    out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
    out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
    out += static_cast<char>(0x80 | (cp & 0x3F));
  }
}

// Decodes the codepoint at text[index] and moves index past it. Malformed
// bytes come back one at a time as U+FFFD.
uint32_t nextCodepoint(const std::string& text, size_t& index) {
  const auto byte = static_cast<unsigned char>(text[index]);
  int extra = 0;
  uint32_t cp = byte;
  if (byte >= 0xF0 && byte < 0xF8) {
    extra = 3;
    cp = byte & 0x07;
  } else if (byte >= 0xE0 && byte < 0xF0) {
    extra = 2;
    cp = byte & 0x0F;
  } else if (byte >= 0xC0 && byte < 0xE0) {
    extra = 1;
    cp = byte & 0x1F;
  } else if (byte >= 0x80) {
    ++index;
    return 0xFFFD;
  }
  if (index + extra >= text.size() + (extra == 0 ? 1 : 0)) {
    ++index;
    return extra == 0 ? cp : 0xFFFD;
  }
  for (int i = 1; i <= extra; ++i) {
    const auto next = static_cast<unsigned char>(text[index + i]);
    if ((next & 0xC0) != 0x80) {
      ++index;
      return 0xFFFD;
    }
    cp = (cp << 6) | (next & 0x3F);
  }
  index += static_cast<size_t>(extra) + 1;
  return cp;
}

bool isArabicScript(const uint32_t cp) {
  return (cp >= 0x0600 && cp <= 0x06FF) || (cp >= 0x0750 && cp <= 0x077F) || (cp >= 0xFB50 && cp <= 0xFDFF) ||
         (cp >= 0xFE70 && cp <= 0xFEFF);
}

std::string trim(const std::string& text) {
  size_t begin = 0;
  size_t end = text.size();
  while (begin < end && static_cast<unsigned char>(text[begin]) <= ' ') ++begin;
  while (end > begin && static_cast<unsigned char>(text[end - 1]) <= ' ') --end;
  return text.substr(begin, end - begin);
}

bool startsWith(const std::string& text, const char* prefix) {
  return text.compare(0, std::strlen(prefix), prefix) == 0;
}

// Cuts text to at most limit bytes on a character boundary and marks the cut.
std::string shortened(const std::string& text, const size_t limit) {
  if (text.size() <= limit) return text;
  size_t end = limit;
  while (end > 0 && (static_cast<unsigned char>(text[end]) & 0xC0) == 0x80) --end;
  // Prefer to stop after a whole word.
  const size_t space = text.rfind(' ', end);
  if (space != std::string::npos && space > end / 2) end = space;
  return text.substr(0, end) + "\xE2\x80\xA6";
}

std::string escapeHtml(const std::string& text) {
  std::string out;
  out.reserve(text.size() + 8);
  for (const char c : text) {
    if (c == '&') {
      out += "&amp;";
    } else if (c == '<') {
      out += "&lt;";
    } else if (c == '>') {
      out += "&gt;";
    } else {
      out += c;
    }
  }
  return out;
}

// Text of an HTML fragment: tags dropped, the common entities decoded, runs of
// white space folded into one space.
std::string stripTags(const std::string& html) {
  std::string out;
  out.reserve(html.size());
  bool inTag = false;
  bool lastWasSpace = true;
  for (size_t i = 0; i < html.size(); ++i) {
    const char c = html[i];
    if (inTag) {
      if (c == '>') inTag = false;
      continue;
    }
    if (c == '<') {
      inTag = true;
      continue;
    }
    uint32_t decoded = 0;
    if (c == '&') {
      const size_t semicolon = html.find(';', i);
      if (semicolon != std::string::npos && semicolon - i <= 9) {
        const std::string name = html.substr(i + 1, semicolon - i - 1);
        if (name == "amp") {
          decoded = '&';
        } else if (name == "lt") {
          decoded = '<';
        } else if (name == "gt") {
          decoded = '>';
        } else if (name == "quot") {
          decoded = '"';
        } else if (name == "apos" || name == "#39") {
          decoded = '\'';
        } else if (name == "nbsp") {
          decoded = ' ';
        } else if (name.size() > 1 && name[0] == '#') {
          const bool hex = name[1] == 'x' || name[1] == 'X';
          decoded = static_cast<uint32_t>(std::strtoul(name.c_str() + (hex ? 2 : 1), nullptr, hex ? 16 : 10));
        }
        if (decoded != 0) i = semicolon;
      }
    }
    if (decoded == 0) decoded = static_cast<unsigned char>(c);
    if (decoded <= ' ' || (decoded == 0xA0 && c == '&')) {
      if (!lastWasSpace) out += ' ';
      lastWasSpace = true;
    } else if (decoded < 0x80 || c == '&') {
      appendUtf8(out, decoded);
      lastWasSpace = false;
    } else {
      out += c;  // a byte of a UTF-8 sequence
      lastWasSpace = false;
    }
  }
  return trim(out);
}

std::string lowered(std::string text) {
  for (char& c : text) {
    if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
  }
  return text;
}

std::vector<std::string> splitLines(const std::string& text) {
  std::vector<std::string> lines;
  size_t start = 0;
  while (start <= text.size()) {
    const size_t end = text.find('\n', start);
    lines.push_back(trim(text.substr(start, end == std::string::npos ? std::string::npos : end - start)));
    if (end == std::string::npos) break;
    start = end + 1;
  }
  return lines;
}

// 0 for an ordinary line, else the heading level ("=== Noun ===" is 3); name gets the heading text.
int headingLevel(const std::string& line, std::string& name) {
  size_t level = 0;
  while (level < line.size() && line[level] == '=') ++level;
  if (level < 2 || line.size() < 2 * level) return 0;
  name = trim(line.substr(level, line.size() - 2 * level));
  return static_cast<int>(level);
}

// "[1] text" and "[2a] text" give "text"; other lines come back empty.
std::string afterSenseMarker(const std::string& line) {
  if (line.empty() || line[0] != '[') return {};
  const size_t close = line.find(']');
  if (close == std::string::npos || close > 8) return {};
  return trim(line.substr(close + 1));
}

const char* footerFor(const Source source) {
  return source == Source::WikipediaSummary ? "<p><i>Wikipedia (CC BY-SA 4.0)</i></p>"
                                            : "<p><i>Wiktionary (CC BY-SA 4.0)</i></p>";
}

// One part of speech gathered from a plain-text entry page.
struct Section {
  std::string name;
  std::string pronunciation;
  std::vector<std::string> senses;
  std::vector<std::string> examples;
  std::string synonyms;
  std::string antonyms;
};

void appendSection(std::string& html, const Section& section, const char* synonymsLabel, const char* antonymsLabel) {
  if (section.senses.empty()) return;
  html += "<p><i>" + escapeHtml(section.name) + "</i>";
  if (!section.pronunciation.empty()) html += " " + escapeHtml(section.pronunciation);
  for (size_t i = 0; i < section.senses.size(); ++i) {
    html += "<br><b>" + std::to_string(i + 1) + ".</b> " + escapeHtml(section.senses[i]);
  }
  for (const std::string& example : section.examples) html += "<br><i>" + escapeHtml(example) + "</i>";
  html += "</p>";
  if (!section.synonyms.empty()) {
    html += std::string("<p><b>") + synonymsLabel + ":</b> " + escapeHtml(section.synonyms) + "</p>";
  }
  if (!section.antonyms.empty()) {
    html += std::string("<p><b>") + antonymsLabel + ":</b> " + escapeHtml(section.antonyms) + "</p>";
  }
}

// de.wiktionary pages list their fields as "Label:" lines followed by "[n] text" lines.
std::string germanEntry(const std::string& text) {
  enum class Field { Other, Pronunciation, Senses, Examples, Synonyms, Antonyms };
  // Labels that start a field. Any other "word:" line is a sub-label inside
  // the current field ("intransitiv:" under Bedeutungen) and changes nothing.
  static const char* const kOtherFields[] = {
      "Worttrennung:", "Herkunft:", "Oberbegriffe:", "Unterbegriffe:", "Redewendungen:", "Sprichw\xC3\xB6rter:",
      "Charakteristische Wortkombinationen:", "Wortbildungen:", "Abk\xC3\xBCrzungen:", "Symbole:",
      "Sinnverwandte W\xC3\xB6rter:", "Verkleinerungsformen:", "Vergr\xC3\xB6\xC3\x9F" "erungsformen:",
      "Weibliche Wortformen:", "M\xC3\xA4nnliche Wortformen:", "Nebenformen:", "Alternative Schreibweisen:",
      "Nicht mehr g\xC3\xBCltige Schreibweisen:", "Entlehnungen:", "Anmerkung:", "Bekannte Namenstr\xC3\xA4ger:",
      "Kurzformen:", "Koseformen:", "Grammatische Merkmale:", "Reime:", "H\xC3\xB6rbeispiele:",
  };

  std::string html;
  Section section;
  Field field = Field::Other;
  int languages = 0;
  int sections = 0;
  bool inSection = false;
  const auto flush = [&] {
    if (inSection) appendSection(html, section, "Synonyme", "Gegenw\xC3\xB6rter");
    section = Section{};
    inSection = false;
  };

  for (const std::string& line : splitLines(text)) {
    if (line.empty()) continue;
    std::string name;
    const int level = headingLevel(line, name);
    if (level == 2) {
      if (++languages > 1) break;
      continue;
    }
    if (level == 3) {
      flush();
      if (++sections > kMaxSections) break;
      section.name = name;
      inSection = true;
      field = Field::Other;
      continue;
    }
    if (level >= 4) {
      field = Field::Other;  // translations, references
      continue;
    }
    if (!inSection) continue;

    if (line.back() == ':' && line[0] != '[') {
      if (line == "Aussprache:") {
        field = Field::Pronunciation;
      } else if (line == "Bedeutungen:") {
        field = Field::Senses;
      } else if (line == "Beispiele:") {
        field = Field::Examples;
      } else if (line == "Synonyme:") {
        field = Field::Synonyms;
      } else if (line == "Gegenw\xC3\xB6rter:") {
        field = Field::Antonyms;
      } else {
        for (const char* known : kOtherFields) {
          if (line == known) field = Field::Other;
        }
      }
      continue;
    }

    const size_t senseLimit = sections == 1 ? kFirstSectionSenses : kLaterSectionSenses;
    switch (field) {
      case Field::Pronunciation:
        if (section.pronunciation.empty() && startsWith(line, "IPA:")) {
          const size_t open = line.find('[');
          const size_t close = line.find(']', open == std::string::npos ? 0 : open);
          if (open != std::string::npos && close != std::string::npos) {
            section.pronunciation = line.substr(open, close - open + 1);
          }
        }
        break;
      case Field::Senses: {
        const std::string sense = afterSenseMarker(line);
        if (!sense.empty() && section.senses.size() < senseLimit) {
          section.senses.push_back(shortened(sense, kMaxSenseBytes));
        }
        break;
      }
      case Field::Examples: {
        const std::string example = afterSenseMarker(line);
        // The shortest sentences first come to hand read best on a small page.
        if (!example.empty() && example.size() <= kMaxExampleBytes && section.examples.size() < kMaxExamples) {
          section.examples.push_back(example);
        }
        break;
      }
      case Field::Synonyms:
        if (section.synonyms.empty()) section.synonyms = shortened(afterSenseMarker(line), 160);
        break;
      case Field::Antonyms:
        if (section.antonyms.empty()) section.antonyms = shortened(afterSenseMarker(line), 160);
        break;
      case Field::Other:
        break;
    }
  }
  flush();
  return html;
}

// fa.wiktionary pages put the senses as plain lines under a part-of-speech heading.
std::string persianEntry(const std::string& text, const std::string& word) {
  // Headings that are not parts of speech: etymology, pronunciation, sources, translations, see also.
  static const char* const kSkippedHeadings[] = {
      "\xD8\xB1\xDB\x8C\xD8\xB4\xD9\x87 \xD9\x84\xD8\xBA\xD8\xAA",                              // ریشه لغت
      "\xD8\xB1\xDB\x8C\xD8\xB4\xD9\x87\xE2\x80\x8C\xD8\xB4\xD9\x86\xD8\xA7\xD8\xB3\xDB\x8C",    // ریشه‌شناسی
      "\xD8\xB1\xDB\x8C\xD8\xB4\xD9\x87 \xD8\xB4\xD9\x86\xD8\xA7\xD8\xB3\xDB\x8C",              // ریشه شناسی
      "\xD8\xA2\xD9\x88\xD8\xA7\xDB\x8C\xD8\xB4",                                                // آوایش
      "\xD8\xAA\xD9\x84\xD9\x81\xD8\xB8",                                                        // تلفظ
      "\xD9\x85\xD9\x86\xD8\xA7\xD8\xA8\xD8\xB9",                                                // منابع
      "\xD9\x85\xD9\x86\xD8\xA8\xD8\xB9",                                                        // منبع
      "\xD8\xA8\xD8\xB1\xDA\xAF\xD8\xB1\xD8\xAF\xD8\xA7\xD9\x86\xE2\x80\x8C\xD9\x87\xD8\xA7",    // برگردان‌ها
      "\xD8\xA8\xD8\xB1\xDA\xAF\xD8\xB1\xD8\xAF\xD8\xA7\xD9\x86 \xD9\x87\xD8\xA7",              // برگردان ها
      "\xD8\xAA\xD8\xB1\xD8\xAC\xD9\x85\xD9\x87\xE2\x80\x8C\xD9\x87\xD8\xA7",                    // ترجمه‌ها
      "\xD9\xBE\xD8\xA7\xD9\x86\xD9\x88\xDB\x8C\xD8\xB3",                                        // پانویس
      "\xD8\xAC\xD8\xB3\xD8\xAA\xD8\xA7\xD8\xB1\xD9\x87\xD8\xA7\xDB\x8C \xD9\x88\xD8\xA7\xD8\xA8\xD8\xB3\xD8\xAA\xD9\x87",  // جستارهای وابسته
  };

  std::string html;
  Section section;
  int languages = 0;
  int sections = 0;
  bool inSection = false;
  const auto flush = [&] {
    if (inSection) appendSection(html, section, "", "");
    section = Section{};
    inSection = false;
  };

  for (const std::string& line : splitLines(text)) {
    if (line.empty()) continue;
    std::string name;
    const int level = headingLevel(line, name);
    if (level == 2) {
      if (++languages > 1) break;
      continue;
    }
    if (level >= 3) {
      flush();
      bool skipped = level > 3;
      for (const char* heading : kSkippedHeadings) skipped = skipped || name == heading;
      if (skipped) continue;
      if (++sections > kMaxSections) break;
      section.name = name;
      inSection = true;
      continue;
    }
    if (!inSection) continue;

    // Not a sense: the headword repeated on its own line, and rules made of dashes.
    if (normalizeWord(line) == word) continue;
    bool onlyDashes = true;
    for (size_t i = 0; i < line.size();) {
      const uint32_t cp = nextCodepoint(line, i);
      if (cp != '-' && cp != 0x2013 && cp != 0x2014 && cp != ' ') onlyDashes = false;
    }
    if (onlyDashes) continue;

    const size_t senseLimit = sections == 1 ? kFirstSectionSenses : kLaterSectionSenses;
    if (section.senses.size() < senseLimit) section.senses.push_back(shortened(line, kMaxSenseBytes));
  }
  flush();
  return html;
}

}  // namespace

const char* code(const Lang lang) {
  switch (lang) {
    case Lang::German:
      return "de";
    case Lang::Persian:
      return "fa";
    case Lang::English:
      break;
  }
  return "en";
}

bool langFromCode(const char* text, Lang& lang) {
  if (!text) return false;
  if (std::strncmp(text, "en", 2) == 0) {
    lang = Lang::English;
  } else if (std::strncmp(text, "de", 2) == 0) {
    lang = Lang::German;
  } else if (std::strncmp(text, "fa", 2) == 0) {
    lang = Lang::Persian;
  } else {
    return false;
  }
  return true;
}

Lang languageFor(const std::string& word, const char* dictionaryLang) {
  for (size_t i = 0; i < word.size();) {
    if (isArabicScript(nextCodepoint(word, i))) return Lang::Persian;
  }
  Lang lang = Lang::English;
  // A Persian dictionary says nothing about a word written in Latin letters.
  if (langFromCode(dictionaryLang, lang) && lang != Lang::Persian) return lang;
  return Lang::English;
}

std::string normalizeWord(const std::string& word) {
  const std::string trimmed = trim(word);
  std::string out;
  out.reserve(trimmed.size());
  for (size_t i = 0; i < trimmed.size();) {
    uint32_t cp = nextCodepoint(trimmed, i);
    if ((cp >= 0x064B && cp <= 0x0652) || cp == 0x0640 || cp == 0x0670) continue;  // vowel marks, tatweel
    if (cp == 0x064A || cp == 0x0649) cp = 0x06CC;                                 // Arabic yeh, alef maksura
    if (cp == 0x0643) cp = 0x06A9;                                                 // Arabic kaf
    appendUtf8(out, cp);
  }
  return out;
}

std::string otherCase(const std::string& word) {
  if (word.empty()) return {};
  std::string other = word;
  if (other[0] >= 'a' && other[0] <= 'z') {
    other[0] = static_cast<char>(other[0] - 'a' + 'A');
  } else if (other[0] >= 'A' && other[0] <= 'Z') {
    other[0] = static_cast<char>(other[0] - 'A' + 'a');
  } else {
    return {};
  }
  return other;
}

std::string percentEncode(const std::string& text) {
  static const char kHex[] = "0123456789ABCDEF";
  std::string out;
  out.reserve(text.size() * 3);
  for (const char c : text) {
    const auto byte = static_cast<unsigned char>(c);
    if ((byte >= 'A' && byte <= 'Z') || (byte >= 'a' && byte <= 'z') || (byte >= '0' && byte <= '9') || byte == '-' ||
        byte == '_' || byte == '.' || byte == '~') {
      out += c;
    } else {
      out += '%';
      out += kHex[byte >> 4];
      out += kHex[byte & 0x0F];
    }
  }
  return out;
}

std::string definitionUrl(const Lang lang, const std::string& word) {
  if (lang == Lang::English) {
    return "https://en.wiktionary.org/api/rest_v1/page/definition/" + percentEncode(word);
  }
  return std::string("https://") + code(lang) +
         ".wiktionary.org/w/api.php?action=query&prop=extracts&explaintext=1&format=json&redirects=1&titles=" +
         percentEncode(word);
}

std::string summaryUrl(const Lang lang, const std::string& word) {
  std::string title = word;
  for (char& c : title) {
    if (c == ' ') c = '_';
  }
  return std::string("https://") + code(lang) + ".wikipedia.org/api/rest_v1/page/summary/" + percentEncode(title);
}

std::string probeUrl(const Lang lang) {
  return std::string("https://") + code(lang) + ".wiktionary.org/w/api.php?action=query&format=json&titles=A";
}

Extractor::Extractor(const Source source, const Lang lang, std::string word)
    : source_(source), lang_(lang), word_(std::move(word)) {
  tokenLimit_ = source == Source::WiktionaryExtract ? 24000 : 4000;
  token_.reserve(256);
}

void Extractor::appendCodepoint(const uint32_t codepoint) {
  if (token_.size() + 4 <= tokenLimit_) appendUtf8(token_, codepoint);
}

void Extractor::feed(const char* data, const size_t length) {
  for (size_t i = 0; i < length; ++i) {
    const char c = data[i];
    if (inString_) {
      if (unicodeDigits_ > 0) {
        uint32_t digit = 0;
        if (c >= '0' && c <= '9') {
          digit = static_cast<uint32_t>(c - '0');
        } else if (c >= 'a' && c <= 'f') {
          digit = static_cast<uint32_t>(c - 'a' + 10);
        } else if (c >= 'A' && c <= 'F') {
          digit = static_cast<uint32_t>(c - 'A' + 10);
        }
        unicodeValue_ = (unicodeValue_ << 4) | digit;
        if (--unicodeDigits_ == 0) {
          if (unicodeValue_ >= 0xD800 && unicodeValue_ <= 0xDBFF) {
            highSurrogate_ = unicodeValue_;
          } else if (unicodeValue_ >= 0xDC00 && unicodeValue_ <= 0xDFFF && highSurrogate_ != 0) {
            appendCodepoint(0x10000 + ((highSurrogate_ - 0xD800) << 10) + (unicodeValue_ - 0xDC00));
            highSurrogate_ = 0;
          } else {
            highSurrogate_ = 0;
            appendCodepoint(unicodeValue_);
          }
        }
      } else if (escaped_) {
        escaped_ = false;
        if (c == 'u') {
          unicodeDigits_ = 4;
          unicodeValue_ = 0;
        } else if (c == 'n') {
          appendCodepoint('\n');
        } else if (c == 't') {
          appendCodepoint(' ');
        } else if (c != 'r' && c != 'b' && c != 'f') {
          appendCodepoint(static_cast<unsigned char>(c));
        }
      } else if (c == '\\') {
        escaped_ = true;
      } else if (c == '"') {
        inString_ = false;
        stringPending_ = true;
      } else if (token_.size() < tokenLimit_) {
        token_ += c;
      }
      continue;
    }

    if (stringPending_) {
      if (c == ' ' || c == '\t' || c == '\n' || c == '\r') continue;
      stringPending_ = false;
      if (c == ':') {
        onKey();
        continue;
      }
      onString();
      // fall through: c is the ',' or the bracket after the value
    }
    if (c == '"') {
      inString_ = true;
      token_.clear();
    } else if (c == '{' || c == '[') {
      ++depth_;
    } else if (c == '}' || c == ']') {
      --depth_;
    }
  }
}

void Extractor::closeSection() {
  if (sectionOpen_) html_ += "</p>";
  sectionOpen_ = false;
}

void Extractor::onKey() {
  key_ = token_;
  if (source_ == Source::WiktionaryDefinitions && depth_ == 1) {
    closeSection();
    topKey_ = key_;
  } else if (source_ == Source::WiktionaryExtract && key_ == "missing") {
    unusable_ = true;
  }
}

void Extractor::onString() {
  switch (source_) {
    case Source::WiktionaryDefinitions: {
      if (topKey_ != code(lang_) || html_.size() > kMaxEntryBytes) return;
      if (key_ == "partOfSpeech") {
        closeSection();
        if (sections_ >= kMaxSections) return;
        ++sections_;
        senses_ = 0;
        examples_ = 0;
        exampleForSense_ = true;
        sectionOpen_ = true;
        html_ += "<p><i>" + escapeHtml(lowered(token_)) + "</i>";
      } else if (key_ == "definition" && sectionOpen_) {
        // Senses past the limit are dropped, and so are their examples.
        exampleForSense_ = true;
        const std::string sense = stripTags(token_);
        if (sense.empty() || senses_ >= (sections_ == 1 ? kFirstSectionSenses : kLaterSectionSenses)) return;
        ++senses_;
        exampleForSense_ = false;
        html_ += "<br><b>" + std::to_string(senses_) + ".</b> " + escapeHtml(shortened(sense, kMaxSenseBytes));
      } else if (key_ == "examples" && sectionOpen_ && !exampleForSense_ && examples_ < kMaxExamples) {
        const std::string example = stripTags(token_);
        if (example.empty() || example.size() > kMaxExampleBytes) return;
        ++examples_;
        exampleForSense_ = true;
        html_ += "<br><i>" + escapeHtml(example) + "</i>";
      }
      return;
    }
    case Source::WiktionaryExtract:
      if (key_ == "extract" && extract_.empty()) extract_ = token_;
      return;
    case Source::WikipediaSummary:
      if (depth_ != 1) return;
      if (key_ == "type") {
        if (token_ != "standard") unusable_ = true;  // disambiguation pages and error answers
      } else if (key_ == "title") {
        title_ = token_;
      } else if (key_ == "description") {
        description_ = token_;
      } else if (key_ == "extract") {
        extract_ = token_;
      }
      return;
  }
}

bool Extractor::finish(std::string& html) {
  if (stringPending_) {
    stringPending_ = false;
    onString();
  }
  std::string body;
  switch (source_) {
    case Source::WiktionaryDefinitions:
      closeSection();
      body = html_;
      // A section whose senses were all empty is only a heading.
      if (body.find("<b>1.</b>") == std::string::npos) body.clear();
      break;
    case Source::WiktionaryExtract:
      if (unusable_ || extract_.empty()) return false;
      body = lang_ == Lang::Persian ? persianEntry(extract_, word_) : germanEntry(extract_);
      if (body.empty()) {
        // An entry laid out in a way these rules do not know: show its opening lines as they are.
        std::string plain;
        for (const std::string& line : splitLines(extract_)) {
          std::string name;
          if (line.empty() || headingLevel(line, name) != 0) continue;
          plain += (plain.empty() ? "" : " ") + line;
          if (plain.size() > 600) break;
        }
        if (plain.empty()) return false;
        body = "<p>" + escapeHtml(shortened(plain, 600)) + "</p>";
      }
      break;
    case Source::WikipediaSummary:
      if (unusable_ || extract_.empty()) return false;
      body = "<p><b>" + escapeHtml(title_.empty() ? word_ : title_) + "</b>";
      if (!description_.empty()) body += " (" + escapeHtml(description_) + ")";
      body += "<br>" + escapeHtml(shortened(extract_, kMaxSummaryBytes)) + "</p>";
      break;
  }
  if (body.empty()) return false;
  html = body + footerFor(source_);
  return true;
}

}  // namespace onlinedict
