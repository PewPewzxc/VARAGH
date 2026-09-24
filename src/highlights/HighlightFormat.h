#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

// Pure, host-testable pieces of the Highlights store: the on-card line format,
// the flashcard level rules, and definition HTML -> plain text. No SD access.
namespace HighlightFormat {

// ---------------------------------------------------------------------------
// Line format: one entry per line, fields separated by TAB. TAB, LF, CR and
// backslash inside a field are escaped as \t \n \r \\ so a line never breaks.
// ---------------------------------------------------------------------------

inline void appendEscaped(std::string& out, std::string_view field) {
  for (const char c : field) {
    switch (c) {
      case '\t':
        out += "\\t";
        break;
      case '\n':
        out += "\\n";
        break;
      case '\r':
        out += "\\r";
        break;
      case '\\':
        out += "\\\\";
        break;
      default:
        out += c;
    }
  }
}

inline std::string joinFields(const std::vector<std::string_view>& fields) {
  std::string out;
  size_t reserve = fields.size();
  for (const auto f : fields) reserve += f.size();
  out.reserve(reserve);
  for (size_t i = 0; i < fields.size(); ++i) {
    if (i) out += '\t';
    appendEscaped(out, fields[i]);
  }
  return out;
}

// Split one line (without its trailing newline) into unescaped fields.
inline std::vector<std::string> splitFields(std::string_view line) {
  std::vector<std::string> fields(1);
  for (size_t i = 0; i < line.size(); ++i) {
    const char c = line[i];
    if (c == '\t') {
      fields.emplace_back();
    } else if (c == '\\' && i + 1 < line.size()) {
      const char n = line[++i];
      fields.back() += n == 't' ? '\t' : n == 'n' ? '\n' : n == 'r' ? '\r' : n;
    } else if (c != '\r') {
      fields.back() += c;
    }
  }
  return fields;
}

// Header line of a category file: "HL1<TAB>type<TAB>name".
constexpr std::string_view HEADER_MAGIC = "HL1";
enum class CategoryType : uint8_t { Notes = 0, Flashcards = 1 };

inline std::string headerLine(const CategoryType type, std::string_view name) {
  const char* typeName = type == CategoryType::Flashcards ? "cards" : "notes";
  return joinFields({HEADER_MAGIC, typeName, name});
}

inline bool parseHeader(std::string_view line, CategoryType& type, std::string& name) {
  const auto f = splitFields(line);
  if (f.size() < 3 || f[0] != HEADER_MAGIC) return false;
  type = f[1] == "cards" ? CategoryType::Flashcards : CategoryType::Notes;
  name = f[2];
  return true;
}

// Entry line: text, meaning, context sentence, source (book), added-at seconds.
struct Entry {
  std::string text;
  std::string meaning;
  std::string context;
  std::string source;
  uint32_t addedAt = 0;
};

inline std::string entryLine(const Entry& e) {
  const std::string added = std::to_string(e.addedAt);
  return joinFields({e.text, e.meaning, e.context, e.source, added});
}

inline bool parseEntry(std::string_view line, Entry& out) {
  auto f = splitFields(line);
  if (f.empty() || f[0].empty()) return false;
  f.resize(5);
  out.text = std::move(f[0]);
  out.meaning = std::move(f[1]);
  out.context = std::move(f[2]);
  out.source = std::move(f[3]);
  out.addedAt = 0;
  for (const char c : f[4]) {
    if (c < '0' || c > '9') break;
    out.addedAt = out.addedAt * 10 + static_cast<uint32_t>(c - '0');
  }
  return true;
}

// ---------------------------------------------------------------------------
// Flashcards: Leitner boxes 1..5 (0 = new, never answered).
// "Again" sends a card back to box 1; "Know it" moves it up one box. Review
// order shows lower boxes first, so cards you miss come back sooner.
// ---------------------------------------------------------------------------
constexpr uint8_t MAX_LEVEL = 5;
constexpr uint8_t KNOWN_LEVEL = 4;  // counted as "known" from box 4 up

inline uint8_t levelAfterAnswer(const uint8_t level, const bool knewIt) {
  if (!knewIt) return 1;
  const uint8_t next = static_cast<uint8_t>((level == 0 ? 1 : level) + 1);
  return next > MAX_LEVEL ? MAX_LEVEL : next;
}

// Stable review order: box ascending (new cards count as box 0), then the
// order the cards were added.
inline std::vector<uint16_t> reviewOrder(const std::vector<uint8_t>& levels) {
  std::vector<uint16_t> order;
  order.reserve(levels.size());
  for (uint8_t box = 0; box <= MAX_LEVEL; ++box) {
    for (size_t i = 0; i < levels.size(); ++i) {
      if ((levels[i] > MAX_LEVEL ? MAX_LEVEL : levels[i]) == box) order.push_back(static_cast<uint16_t>(i));
    }
  }
  return order;
}

// ---------------------------------------------------------------------------
// Dictionary definition HTML -> compact plain text for a flashcard back.
// Block tags become line breaks, all other tags are dropped, the common
// entities are decoded, runs of spaces collapse, and the result is cut at a
// UTF-8 boundary to at most maxBytes.
// ---------------------------------------------------------------------------
inline std::string plainTextFromDefinition(std::string_view html, const size_t maxBytes) {
  std::string out;
  out.reserve(html.size() < maxBytes ? html.size() : maxBytes);
  const auto pushChar = [&out](const char c) {
    if (c == ' ' || c == '\t') {
      if (!out.empty() && out.back() != ' ' && out.back() != '\n') out += ' ';
    } else if (c == '\n') {
      while (!out.empty() && out.back() == ' ') out.pop_back();
      if (!out.empty() && out.back() != '\n') out += '\n';
    } else {
      out += c;
    }
  };
  size_t i = 0;
  while (i < html.size() && out.size() < maxBytes) {
    const char c = html[i];
    if (c == '<') {
      const size_t end = html.find('>', i);
      if (end == std::string_view::npos) break;
      std::string_view tag = html.substr(i + 1, end - i - 1);
      if (!tag.empty() && tag[0] == '/') tag.remove_prefix(1);
      size_t nameLen = 0;
      while (nameLen < tag.size() && ((tag[nameLen] >= 'a' && tag[nameLen] <= 'z') ||
                                      (tag[nameLen] >= 'A' && tag[nameLen] <= 'Z') ||
                                      (tag[nameLen] >= '0' && tag[nameLen] <= '9'))) {
        ++nameLen;
      }
      std::string name(tag.substr(0, nameLen));
      for (auto& ch : name) ch = static_cast<char>(ch >= 'A' && ch <= 'Z' ? ch + 32 : ch);
      if (name == "br" || name == "p" || name == "div" || name == "li" || name == "tr" || name == "hr" ||
          (name.size() == 2 && name[0] == 'h' && name[1] >= '1' && name[1] <= '6')) {
        pushChar('\n');
      }
      i = end + 1;
    } else if (c == '&') {
      const size_t end = html.find(';', i);
      const std::string_view entity =
          end != std::string_view::npos && end - i <= 8 ? html.substr(i + 1, end - i - 1) : std::string_view{};
      const char* decoded = entity == "lt"     ? "<"
                            : entity == "gt"   ? ">"
                            : entity == "amp"  ? "&"
                            : entity == "quot" ? "\""
                            : entity == "apos" ? "'"
                            : entity == "#39"  ? "'"
                            : entity == "nbsp" ? " "
                                               : nullptr;
      if (decoded) {
        for (const char* p = decoded; *p; ++p) pushChar(*p);
        i = end + 1;
      } else {
        pushChar(c);
        ++i;
      }
    } else {
      pushChar(c == '\r' ? ' ' : c);
      ++i;
    }
  }
  if (out.size() > maxBytes) out.resize(maxBytes);
  // Never leave half a UTF-8 sequence at the end: drop an incomplete last one.
  size_t start = out.size();
  while (start > 0 && (static_cast<unsigned char>(out[start - 1]) & 0xC0) == 0x80) --start;
  if (start > 0) {
    const auto lead = static_cast<unsigned char>(out[start - 1]);
    const size_t need = lead >= 0xF0 ? 4 : lead >= 0xE0 ? 3 : lead >= 0xC0 ? 2 : 1;
    if (out.size() - (start - 1) < need) out.resize(start - 1);
  } else if (!out.empty()) {
    out.clear();  // only continuation bytes: not valid text
  }
  while (!out.empty() && (out.back() == ' ' || out.back() == '\n')) out.pop_back();
  return out;
}

}  // namespace HighlightFormat
