#pragma once

#include <cstdint>
#include <string>
#include <string_view>

// Book ratings as text: the stars shown in menus and lists, and the lines of
// the list of ratings kept on the card. Pure string work, tested on the host.
//
// A rating is a number of half stars: 0 is "not rated", 10 is five stars.
namespace bookrating {

constexpr uint8_t kMaxHalfStars = 10;
constexpr int kStars = 5;

constexpr uint8_t clampHalfStars(const int halfStars) {
  return static_cast<uint8_t>(halfStars < 0 ? 0 : (halfStars > kMaxHalfStars ? kMaxHalfStars : halfStars));
}

// "3.5" or "4"; empty for a book that is not rated.
inline std::string number(const uint8_t halfStars) {
  if (halfStars == 0) return {};
  std::string text = std::to_string(halfStars / 2);
  if (halfStars % 2) text += ".5";
  return text;
}

// Five star symbols of the menu font: full, half, empty.
inline std::string stars(const uint8_t halfStars) {
  std::string text;
  text.reserve(kStars * 3);
  for (int star = 0; star < kStars; ++star) {
    if (halfStars >= (star + 1) * 2) {
      text += "\xE2\x98\x85";  // U+2605
    } else if (halfStars == star * 2 + 1) {
      text += "\xE2\xAF\xAA";  // U+2BEA, the left half filled
    } else {
      text += "\xE2\x98\x86";  // U+2606
    }
  }
  return text;
}

// What a tap on the row of stars makes of the rating: the left half of a star
// counts half of it, the right half all of it, and tapping the value that is
// already set takes the rating away.
constexpr uint8_t afterTap(const uint8_t current, const int starIndex, const bool rightHalf) {
  const uint8_t tapped = clampHalfStars(starIndex * 2 + (rightHalf ? 2 : 1));
  return tapped == current ? 0 : tapped;
}

// "3.5" (or "3,5") is 7; anything that is not a rating is 0. Rounds to the
// nearest half star.
inline uint8_t parseNumber(std::string_view text) {
  while (!text.empty() && (text.front() == ' ' || text.front() == '\t')) text.remove_prefix(1);
  if (text.empty() || text[0] < '0' || text[0] > '9') return 0;
  int whole = 0;
  size_t at = 0;
  while (at < text.size() && text[at] >= '0' && text[at] <= '9' && whole < 100) whole = whole * 10 + (text[at++] - '0');
  int tenth = 0;
  if (at + 1 < text.size() && (text[at] == '.' || text[at] == ',') && text[at + 1] >= '0' && text[at + 1] <= '9') {
    tenth = text[at + 1] - '0';
  }
  return clampHalfStars((whole * 10 + tenth + 2) / 5);
}

// One line of the list: rating, title, author and file, separated by tabs.
struct ListLine {
  uint8_t halfStars = 0;
  std::string_view title;
  std::string_view author;
  std::string_view path;
};

inline std::string formatLine(const uint8_t halfStars, const std::string_view title, const std::string_view author,
                              const std::string_view path) {
  // A tab or a line break inside a name would break the line apart.
  const auto cleaned = [](const std::string_view text) {
    std::string out(text);
    for (char& c : out) {
      if (c == '\t' || c == '\n' || c == '\r') c = ' ';
    }
    return out;
  };
  return number(halfStars) + '\t' + cleaned(title) + '\t' + cleaned(author) + '\t' + cleaned(path);
}

// False for an empty line, a "#" comment, or a line without a rating and a file.
inline bool parseLine(std::string_view line, ListLine& out) {
  while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) line.remove_suffix(1);
  if (line.empty() || line.front() == '#') return false;
  std::string_view fields[4];
  for (int i = 0; i < 4; ++i) {
    const size_t tab = i < 3 ? line.find('\t') : std::string_view::npos;
    if (tab == std::string_view::npos) {
      if (i < 3) return false;
      fields[i] = line;
      break;
    }
    fields[i] = line.substr(0, tab);
    line.remove_prefix(tab + 1);
  }
  out.halfStars = parseNumber(fields[0]);
  out.title = fields[1];
  out.author = fields[2];
  out.path = fields[3];
  return out.halfStars > 0 && !out.path.empty();
}

}  // namespace bookrating
