#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

// VARAGH: Focus Reading for Persian (and other Arabic-script) words.
//
// Bolding "the first part" of a joined Persian word would cut a letter off
// from its neighbour and change both shapes. Persian words already break
// naturally after the letters that never join the next one (ا آ د ذ ر ز ژ و
// and friends) and at the half-space (ZWNJ), so the bold part ends at one of
// those breaks. Vowel marks (zir, zebar, pish, tashdid, ...) stay with their
// letter. "می‌روم" -> bold "می‌", "کتاب" -> bold "کتا".
namespace PersianFocus {

constexpr uint32_t ZWNJ = 0x200C;

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

// Byte offset where the bold part ends, or 0 for no bold part (no natural
// break before the last letter). targetPercent is Focus Reading's share.
constexpr size_t boundary(const std::string_view word, const size_t targetPercent) {
  size_t letters = 0;
  for (size_t i = 0; i < word.size();) {
    if (isArabicLetter(decode(word, i))) ++letters;
  }
  if (letters < 2) return 0;
  size_t target = letters * targetPercent / 100;
  if (target < 1) target = 1;

  size_t best = 0;          // last break with no more than target letters bold
  size_t firstAbove = 0;    // first break past the target
  size_t seen = 0;
  bool breakPending = false;  // after a non-joining letter, waiting for its marks
  for (size_t i = 0; i < word.size();) {
    const size_t at = i;
    const uint32_t cp = decode(word, i);
    if (breakPending && !isMark(cp)) {
      breakPending = false;
      if (seen < letters) {  // a letter must stay outside the bold part
        if (seen <= target) best = at;
        else if (firstAbove == 0) firstAbove = at;
      }
    }
    if (cp == ZWNJ) {
      if (seen > 0 && seen < letters) {
        if (seen <= target) best = i;
        else if (firstAbove == 0) firstAbove = i;
      }
      continue;
    }
    if (isArabicLetter(cp)) {
      ++seen;
      if (isRightJoiningOnly(cp)) breakPending = true;
    }
  }
  return best != 0 ? best : firstAbove;
}

}  // namespace PersianFocus
