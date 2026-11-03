#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string_view>

// VARAGH: Focus Reading for Persian (and other Arabic-script) words.
//
// Like English, the first part of every word is bold (Focus Reading's share of
// its letters, at least one letter). The cut may fall between two joined
// letters: drawing then adds a ZWJ on both sides of the cut so the letters stay
// joined across the bold and regular runs. Vowel marks (zir, zebar, pish,
// tashdid, ...) stay with their letter and a half-space (ZWNJ) stays with the
// bold part. "کتاب" -> bold "ک", "می‌روم" -> bold "می‌", "و" -> all bold.
namespace PersianFocus {

constexpr uint32_t ZWNJ = 0x200C;
// Share of each word's letters drawn bold (rounded down, at least one letter).
constexpr size_t BOLD_PERCENT = 40;

constexpr bool isArabicLetter(const uint32_t cp) {
  return (cp >= 0x0620 && cp <= 0x064A) || (cp >= 0x066E && cp <= 0x06D3) || cp == 0x06D5 ||
         (cp >= 0x06FA && cp <= 0x06FC) || (cp >= 0xFB50 && cp <= 0xFDFF) || (cp >= 0xFE70 && cp <= 0xFEFC);
}

// Marks drawn on a letter: they belong to the letter before them.
constexpr bool isMark(const uint32_t cp) {
  return (cp >= 0x0610 && cp <= 0x061A) || (cp >= 0x064B && cp <= 0x065F) || cp == 0x0670 ||
         (cp >= 0x06D6 && cp <= 0x06ED);
}

// Letters that never join the letter after them.
constexpr bool isRightJoiningOnly(const uint32_t cp) {
  switch (cp) {
    case 0x0621:  // hamza (stands alone)
    case 0x0622:  // alef madda
    case 0x0623:
    case 0x0624:
    case 0x0625:
    case 0x0627:  // alef
    case 0x0629:  // teh marbuta
    case 0x062F:  // dal
    case 0x0630:  // thal
    case 0x0631:  // reh
    case 0x0632:  // zain
    case 0x0648:  // waw
    case 0x0671:
    case 0x0698:  // zhe
    case 0x06C0:
    case 0x06D5:
      return true;
    default:
      return false;
  }
}

constexpr uint32_t decode(const std::string_view s, size_t& i) {
  const auto b = static_cast<unsigned char>(s[i]);
  int extra = b < 0x80 ? 0 : b < 0xE0 ? 1 : b < 0xF0 ? 2 : 3;
  uint32_t cp = b < 0x80 ? b : b < 0xE0 ? (b & 0x1F) : b < 0xF0 ? (b & 0x0F) : (b & 0x07);
  ++i;
  while (extra-- > 0 && i < s.size()) cp = (cp << 6) | (static_cast<unsigned char>(s[i++]) & 0x3F);
  return cp;
}

// True when the word starts (after any leading non-letters) with an
// Arabic-script letter.
constexpr bool startsArabic(const std::string_view word) {
  size_t i = 0;
  while (i < word.size()) {
    const uint32_t cp = decode(word, i);
    if (isArabicLetter(cp)) return true;
    // A letter of another script decides; quotes and other marks are skipped.
    if ((cp | 0x20) - 'a' < 26) return false;
    if ((cp >= 0x00C0 && cp < 0x0600) || (cp >= 0x0700 && cp < 0x2000) || (cp >= 0x3000 && cp < 0xFB50)) return false;
  }
  return false;
}

// Byte offset where the bold part ends; word.size() when the whole word is bold
// (a one-letter word), 0 when the word has no Arabic letter. targetPercent is
// Focus Reading's share.
constexpr size_t boundary(const std::string_view word, const size_t targetPercent) {
  size_t letters = 0;
  for (size_t i = 0; i < word.size();) {
    if (isArabicLetter(decode(word, i))) ++letters;
  }
  if (letters == 0) return 0;
  size_t target = letters * targetPercent / 100;  // rounded down: دانشگاه (7 letters) -> دا
  if (target < 1) target = 1;
  if (target > 9) target = 9;  // same cap as English words
  if (target >= letters) return word.size();

  // Cut right before the letter after the bold ones, so their marks and a
  // half-space come along with them.
  size_t seen = 0;
  for (size_t i = 0; i < word.size();) {
    const size_t at = i;
    if (isArabicLetter(decode(word, i)) && seen++ == target) return at;
  }
  return word.size();
}

// True when the letters on both sides of `cut` join, so the two runs need a
// ZWJ at the cut to keep their joined shapes.
constexpr bool joinsAcross(const std::string_view word, const size_t cut) {
  if (cut == 0 || cut >= word.size()) return false;
  uint32_t before = 0;
  for (size_t i = 0; i < cut;) {
    const uint32_t cp = decode(word, i);
    if (!isMark(cp)) before = cp;
  }
  size_t i = cut;
  const uint32_t after = decode(word, i);
  return isArabicLetter(before) && !isRightJoiningOnly(before) && isArabicLetter(after);
}

// Copies the bold and regular runs of a word split at `cut` into the buffers,
// with a ZWJ on both sides of a joined cut. False when a run does not fit.
inline bool splitRuns(const char* word, const size_t wordLength, const size_t cut, char* bold,
                      const size_t boldCapacity, char* rest, const size_t restCapacity) {
  static constexpr char kZwj[] = "\xE2\x80\x8D";
  const size_t joiner = joinsAcross(std::string_view(word, wordLength), cut) ? 3 : 0;
  if (cut > wordLength || cut + joiner >= boldCapacity || wordLength - cut + joiner >= restCapacity) return false;
  memcpy(bold, word, cut);
  memcpy(bold + cut, kZwj, joiner);
  bold[cut + joiner] = '\0';
  memcpy(rest, kZwj, joiner);
  memcpy(rest + joiner, word + cut, wordLength - cut);
  rest[joiner + wordLength - cut] = '\0';
  return true;
}

}  // namespace PersianFocus
