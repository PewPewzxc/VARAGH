#include "WordForms.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <string_view>
#include <type_traits>
#include <utility>

namespace WordForms {
namespace {

// ---------------------------------------------------------------------------
// English
// ---------------------------------------------------------------------------

// The suffix rules propose candidates per ending; english() then orders them.
std::vector<std::string> englishCandidates(const std::string& word) {
  // These are deliberately English morphology rules. Applying them to UTF-8
  // text can manufacture meaningless variants from another language, so
  // non-ASCII words use exact and StarDict synonym lookup only.
  if (std::any_of(word.begin(), word.end(), [](const unsigned char c) { return c >= 0x80; })) return {};

  std::string normalized = word;
  std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                 [](unsigned char c) { return std::tolower(c); });

  std::vector<std::string> variants;
  variants.reserve(8);
  size_t len = normalized.size();
  if (len < 3) return variants;

  auto endsWith = [&normalized, len](const char* suffix) {
    size_t slen = strlen(suffix);
    return len >= slen && normalized.compare(len - slen, slen, suffix) == 0;
  };

  std::string scratch;
  scratch.reserve(len + 4);
  auto addVariant = [&variants, &scratch, &normalized](size_t keep, const char* suffix) {
    scratch.assign(normalized, 0, keep);
    scratch.append(suffix);
    if (scratch.size() < 2) return;
    if (std::find(variants.begin(), variants.end(), scratch) != variants.end()) return;
    variants.push_back(scratch);
  };
  auto addTail = [&variants, &scratch, &normalized, len](size_t start) {
    scratch.assign(normalized, start, len - start);
    if (scratch.size() < 2) return;
    if (std::find(variants.begin(), variants.end(), scratch) != variants.end()) return;
    variants.push_back(scratch);
  };

  // Plurals
  if (endsWith("sses")) addVariant(len - 2, "");
  if (endsWith("ses")) addVariant(len - 2, "is");
  if (endsWith("ies")) {
    addVariant(len - 3, "y");
    addVariant(len - 2, "");
  }
  if (endsWith("ves")) {
    addVariant(len - 3, "f");
    addVariant(len - 3, "fe");
    addVariant(len - 1, "");
  }
  if (endsWith("men")) addVariant(len - 3, "man");
  if (endsWith("es") && !endsWith("sses") && !endsWith("ies") && !endsWith("ves")) {
    addVariant(len - 2, "");
    addVariant(len - 1, "");
  }
  if (endsWith("s") && !endsWith("ss") && !endsWith("us") && !endsWith("es")) {
    addVariant(len - 1, "");
  }

  // Past tense
  if (endsWith("ied")) {
    addVariant(len - 3, "y");
    addVariant(len - 1, "");
  }
  if (endsWith("ed") && !endsWith("ied")) {
    addVariant(len - 2, "");
    addVariant(len - 1, "");
    if (len > 4 && normalized[len - 3] == normalized[len - 4]) {
      addVariant(len - 3, "");
    }
  }

  // Progressive
  if (endsWith("ying")) {
    addVariant(len - 3, "");  // "crying" -> "cry"
    addVariant(len - 4, "ie");  // "dying" -> "die"
  }
  if (endsWith("ing") && !endsWith("ying")) {
    addVariant(len - 3, "");
    addVariant(len - 3, "e");
    if (len > 5 && normalized[len - 4] == normalized[len - 5]) {
      addVariant(len - 4, "");
    }
  }

  // Adverb
  if (endsWith("ically")) {
    addVariant(len - 6, "ic");
    addVariant(len - 4, "");
  }
  if (endsWith("ally") && !endsWith("ically")) {
    addVariant(len - 4, "al");
    addVariant(len - 2, "");
  }
  if (endsWith("ily") && !endsWith("ally")) {
    addVariant(len - 3, "y");
  }
  if (endsWith("ly") && !endsWith("ily") && !endsWith("ally")) {
    addVariant(len - 2, "");
  }

  // Comparative / superlative
  if (endsWith("ier")) {
    addVariant(len - 3, "y");
  }
  if (endsWith("er") && !endsWith("ier")) {
    addVariant(len - 2, "");
    addVariant(len - 1, "");
    if (len > 4 && normalized[len - 3] == normalized[len - 4]) {
      addVariant(len - 3, "");
    }
  }
  if (endsWith("iest")) {
    addVariant(len - 4, "y");
  }
  if (endsWith("est") && !endsWith("iest")) {
    addVariant(len - 3, "");
    addVariant(len - 2, "");
    if (len > 5 && normalized[len - 4] == normalized[len - 5]) {
      addVariant(len - 4, "");
    }
  }

  // Derivational suffixes
  if (endsWith("ness")) addVariant(len - 4, "");
  if (endsWith("ment")) addVariant(len - 4, "");
  if (endsWith("ful")) addVariant(len - 3, "");
  if (endsWith("less")) addVariant(len - 4, "");
  if (endsWith("able")) {
    addVariant(len - 4, "");
    addVariant(len - 4, "e");
  }
  if (endsWith("ible")) {
    addVariant(len - 4, "");
    addVariant(len - 4, "e");
  }
  if (endsWith("ation")) {
    addVariant(len - 5, "");
    addVariant(len - 5, "e");
    addVariant(len - 5, "ate");
  }
  if (endsWith("tion") && !endsWith("ation")) {
    addVariant(len - 4, "te");
    addVariant(len - 3, "");
    addVariant(len - 3, "e");
  }
  if (endsWith("ion") && !endsWith("tion")) {
    addVariant(len - 3, "");
    addVariant(len - 3, "e");
  }
  if (endsWith("al") && !endsWith("ial")) {
    addVariant(len - 2, "");
    addVariant(len - 2, "e");
  }
  if (endsWith("ial")) {
    addVariant(len - 3, "");
    addVariant(len - 3, "e");
  }
  if (endsWith("ous")) {
    addVariant(len - 3, "");
    addVariant(len - 3, "e");
  }
  if (endsWith("ive")) {
    addVariant(len - 3, "");
    addVariant(len - 3, "e");
  }
  if (endsWith("ize")) {
    addVariant(len - 3, "");
    addVariant(len - 3, "e");
  }
  if (endsWith("ise")) {
    addVariant(len - 3, "");
    addVariant(len - 3, "e");
  }
  if (endsWith("en")) {
    addVariant(len - 2, "");
    addVariant(len - 2, "e");
  }

  // Prefix removal
  if (len > 5 && normalized.compare(0, 2, "un") == 0) addTail(2);
  if (len > 6 && normalized.compare(0, 3, "dis") == 0) addTail(3);
  if (len > 6 && normalized.compare(0, 3, "mis") == 0) addTail(3);
  if (len > 6 && normalized.compare(0, 3, "pre") == 0) addTail(3);
  if (len > 7 && normalized.compare(0, 4, "over") == 0) addTail(4);
  if (len > 5 && normalized.compare(0, 2, "re") == 0) addTail(2);

  return variants;
}

bool isVowel(const char c) { return c == 'a' || c == 'e' || c == 'i' || c == 'o' || c == 'u'; }

// One vowel group ending consonant-vowel-consonant ("hop", "mak", "us"), the
// shape whose -ing/-ed/-er form drops a silent final e: "hoping" is "hope"
// while "hopping" is "hop". Final w, x and y never take that e.
bool isShortClosedSyllable(const std::string& stem) {
  const size_t n = stem.size();
  if (n < 2 || n > 4) return false;
  const char last = stem[n - 1];
  if (isVowel(last) || last == 'w' || last == 'x' || last == 'y') return false;
  if (!isVowel(stem[n - 2])) return false;
  if (n >= 3 && isVowel(stem[n - 3])) return false;
  int groups = 0;
  for (size_t i = 0; i < n; ++i) {
    if (isVowel(stem[i]) && (i == 0 || !isVowel(stem[i - 1]))) ++groups;
  }
  return groups == 1;
}

bool endsWith(const std::string& s, const char* suffix) {
  const size_t n = strlen(suffix);
  return s.size() >= n && s.compare(s.size() - n, n, suffix) == 0;
}

// How far a candidate is from the word: letters taken off plus letters added.
int englishCost(const std::string& word, const std::string& candidate) {
  size_t common = 0;
  while (common < word.size() && common < candidate.size() && word[common] == candidate[common]) ++common;
  int cost = static_cast<int>((word.size() - common) + (candidate.size() - common));
  // "hoping" -> "hope", "coded" -> "code", "later" -> "late" before the stem alone.
  if (candidate.size() >= 3 && candidate.back() == 'e' && candidate.size() < word.size() &&
      (endsWith(word, "ing") || endsWith(word, "ed") || endsWith(word, "er") || endsWith(word, "est")) &&
      word.compare(0, candidate.size() - 1, candidate, 0, candidate.size() - 1) == 0 &&
      isShortClosedSyllable(candidate.substr(0, candidate.size() - 1))) {
    cost -= 2;
  }
  // After x, z, ch, sh and ss the plural -es is never part of the word itself:
  // "boxes" is "box", not "boxe".
  if (endsWith(word, "es") && candidate.size() + 2 == word.size() &&
      (endsWith(candidate, "x") || endsWith(candidate, "z") || endsWith(candidate, "ch") ||
       endsWith(candidate, "sh") || endsWith(candidate, "ss"))) {
    cost -= 2;
  }
  return cost;
}

// ---------------------------------------------------------------------------
// Persian
// ---------------------------------------------------------------------------

constexpr char32_t ZWNJ = 0x200C;

std::u32string decode(const std::string& s) {
  std::u32string out;
  out.reserve(s.size());
  for (size_t i = 0; i < s.size();) {
    const auto c = static_cast<unsigned char>(s[i]);
    char32_t cp = c;
    size_t len = 1;
    if (c >= 0xF0 && i + 3 < s.size()) {
      cp = ((c & 0x07u) << 18) | ((s[i + 1] & 0x3Fu) << 12) | ((s[i + 2] & 0x3Fu) << 6) | (s[i + 3] & 0x3Fu);
      len = 4;
    } else if (c >= 0xE0 && i + 2 < s.size()) {
      cp = ((c & 0x0Fu) << 12) | ((s[i + 1] & 0x3Fu) << 6) | (s[i + 2] & 0x3Fu);
      len = 3;
    } else if (c >= 0xC0 && i + 1 < s.size()) {
      cp = ((c & 0x1Fu) << 6) | (s[i + 1] & 0x3Fu);
      len = 2;
    }
    out.push_back(cp);
    i += len;
  }
  return out;
}

std::string encode(const std::u32string& s) {
  std::string out;
  out.reserve(s.size() * 2);
  for (const char32_t cp : s) {
    if (cp < 0x80) {
      out.push_back(static_cast<char>(cp));
    } else if (cp < 0x800) {
      out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
      out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp < 0x10000) {
      out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
      out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
      out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else {
      out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
      out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
      out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
      out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
  }
  return out;
}

// Vowel marks, tatweel and the Arabic letter forms the Persian dictionaries
// do not use: "كتابي" -> "کتابی", "خانهٔ" -> "خانه".
std::u32string normalizePersian(const std::u32string& w) {
  std::u32string out;
  out.reserve(w.size());
  for (char32_t cp : w) {
    if ((cp >= 0x064B && cp <= 0x0655) || cp == 0x0670 || cp == 0x0640) continue;
    if (cp == 0x064A || cp == 0x0649) cp = 0x06CC;  // yeh
    if (cp == 0x0643) cp = 0x06A9;                  // kaf
    if (cp == 0x06C0) cp = 0x0647;                  // heh with yeh above
    out.push_back(cp);
  }
  while (!out.empty() && (out.back() == ZWNJ || out.back() == U' ')) out.pop_back();
  return out;
}

bool endsWith32(const std::u32string& s, const std::u32string_view suffix) {
  return s.size() >= suffix.size() && s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

bool startsWith32(const std::u32string& s, const std::u32string_view prefix) {
  return s.size() >= prefix.size() && s.compare(0, prefix.size(), prefix) == 0;
}

// Letters only (a half-space is not a letter).
size_t letterCount(const std::u32string& s) {
  return static_cast<size_t>(std::count_if(s.begin(), s.end(), [](const char32_t c) { return c != ZWNJ; }));
}

std::u32string trimJoiner(std::u32string s) {
  while (!s.empty() && s.back() == ZWNJ) s.pop_back();
  return s;
}

struct Candidate {
  std::u32string text;
  int cost;
};

class PersianList {
 public:
  explicit PersianList(const std::u32string& original) : original_(original) {}

  void add(std::u32string text, const int cost) {
    text = trimJoiner(std::move(text));
    if (letterCount(text) < 2 || text == original_) return;
    for (auto& c : list_) {
      if (c.text == text) {
        c.cost = std::min(c.cost, cost);
        return;
      }
    }
    list_.push_back({std::move(text), cost});
  }

  std::vector<std::string> finish() {
    std::stable_sort(list_.begin(), list_.end(), [](const Candidate& a, const Candidate& b) { return a.cost < b.cost; });
    std::vector<std::string> out;
    for (const auto& c : list_) {
      if (out.size() >= MAX_VARIANTS) break;
      out.push_back(encode(c.text));
    }
    return out;
  }

 private:
  std::u32string original_;
  std::vector<Candidate> list_;
};

// Noun endings, one layer each, longest first so "های" is taken whole.
// Possessive, indefinite and ezafe endings:
constexpr std::u32string_view kPossessive[] = {U"هایمان", U"هایتان", U"هایشان", U"مان", U"تان", U"شان", U"ای", U"یی",
                                      U"ام",     U"ات",     U"اش",     U"م",   U"ت",   U"ش",   U"ی"};
// Plural endings: (ending, replacement) so "بندگان" -> "بنده", "کلمات" -> "کلمه".
constexpr std::pair<std::u32string_view, std::u32string_view> kPlural[] = {
    {U"هایی", U""}, {U"های", U""}, {U"ها", U""}, {U"یان", U""}, {U"گان", U"ه"},
    {U"ان", U""},   {U"ات", U""},  {U"ات", U"ه"}, {U"ین", U""},
};
constexpr std::u32string_view kComparative[] = {U"ترین", U"تر"};

// Present stem -> infinitive for common verbs whose infinitive is not the
// regular "stem + یدن". Regular verbs ("پرس" -> "پرسیدن") need no entry.
constexpr std::pair<std::u32string_view, std::u32string_view> kIrregular[] = {
    {U"رو", U"رفتن"},       {U"کن", U"کردن"},       {U"گو", U"گفتن"},       {U"گوی", U"گفتن"},
    {U"بین", U"دیدن"},      {U"خواه", U"خواستن"},   {U"شو", U"شدن"},        {U"ده", U"دادن"},
    {U"گیر", U"گرفتن"},     {U"آ", U"آمدن"},        {U"آی", U"آمدن"},       {U"دان", U"دانستن"},
    {U"توان", U"توانستن"},  {U"نویس", U"نوشتن"},    {U"خوان", U"خواندن"},   {U"زن", U"زدن"},
    {U"نشین", U"نشستن"},    {U"گذار", U"گذاشتن"},   {U"گذر", U"گذشتن"},     {U"دار", U"داشتن"},
    {U"ساز", U"ساختن"},     {U"باش", U"بودن"},      {U"یاب", U"یافتن"},     {U"خور", U"خوردن"},
    {U"بر", U"بردن"},       {U"آور", U"آوردن"},     {U"فرست", U"فرستادن"},  {U"افت", U"افتادن"},
    {U"ایست", U"ایستادن"},  {U"شکن", U"شکستن"},     {U"بند", U"بستن"},      {U"پز", U"پختن"},
    {U"سوز", U"سوختن"},     {U"فروش", U"فروختن"},   {U"آموز", U"آموختن"},   {U"شناس", U"شناختن"},
    {U"نه", U"نهادن"},      {U"مان", U"ماندن"},     {U"میر", U"مردن"},      {U"شمار", U"شمردن"},
    {U"پذیر", U"پذیرفتن"},  {U"جو", U"جستن"},       {U"شوی", U"شستن"},      {U"ریز", U"ریختن"},
    {U"انداز", U"انداختن"}, {U"پرداز", U"پرداختن"}, {U"خیز", U"خاستن"},     {U"گریز", U"گریختن"},
    {U"کاه", U"کاستن"},     {U"زی", U"زیستن"},      {U"گری", U"گریستن"},    {U"نواز", U"نواختن"},
    {U"ران", U"راندن"},     {U"تاب", U"تافتن"},     {U"بخش", U"بخشیدن"},    {U"آمیز", U"آمیختن"},
};
constexpr std::u32string_view kVerbPrefixes[] = {U"نمی‌", U"نمی", U"می‌", U"می", U"ب", U"ن"};
constexpr std::u32string_view kPersonalEndings[] = {U"یم", U"ید", U"ند", U"م", U"ی", U"د"};

// A stem that may be a verb's: add its infinitive candidates.
void addInfinitives(PersianList& list, const std::u32string& stem, const int cost) {
  if (letterCount(stem) < 1) return;
  for (const auto& [present, infinitive] : kIrregular) {
    if (stem == present) list.add(std::u32string(infinitive), cost);
  }
  // Past stem ("رفت", "خورد") or a regular present stem ("پرس").
  if (endsWith32(stem, U"د") || endsWith32(stem, U"ت")) list.add(stem + U"ن", cost + 1);
  if (letterCount(stem) >= 2) list.add(stem + U"یدن", cost + 2);
}

void addPersianNouns(PersianList& list, const std::u32string& w) {
  // Possessive/indefinite, then plural, then comparative endings; each optional.
  std::vector<std::pair<std::u32string, int>> layer{{w, 0}};
  const auto strip = [&](const auto& endings) {
    std::vector<std::pair<std::u32string, int>> next = layer;
    for (const auto& [base, cost] : layer) {
      for (const auto& entry : endings) {
        std::u32string_view ending, replacement;
        if constexpr (std::is_same_v<std::decay_t<decltype(entry)>, std::u32string_view>) {
          ending = entry;
        } else {
          ending = entry.first;
          replacement = entry.second;
        }
        if (!endsWith32(base, ending)) continue;
        std::u32string stem = trimJoiner(base.substr(0, base.size() - ending.size()));
        if (letterCount(stem) < 2) continue;
        const int c = cost + static_cast<int>(ending.size() + replacement.size());
        stem.append(replacement);
        next.push_back({std::move(stem), c});
      }
    }
    layer = std::move(next);
  };
  strip(kPossessive);
  strip(kPlural);
  strip(kComparative);
  for (const auto& [form, cost] : layer) list.add(form, cost);
}

void addPersianVerbs(PersianList& list, const std::u32string& w) {
  for (const auto& prefix : kVerbPrefixes) {
    const bool hasPrefix = !prefix.empty() && startsWith32(w, prefix);
    if (!hasPrefix) continue;
    const std::u32string rest = w.substr(prefix.size());
    if (letterCount(rest) < 1) continue;
    // ب and ن are only verb prefixes when something long enough follows.
    const bool weakPrefix = prefix == U"ب" || prefix == U"ن";
    if (weakPrefix && letterCount(rest) < 3) continue;
    const int base = static_cast<int>(prefix.size()) + (weakPrefix ? 4 : 0);
    addInfinitives(list, rest, base + 2);
    for (const auto& ending : kPersonalEndings) {
      if (!endsWith32(rest, ending)) continue;
      addInfinitives(list, rest.substr(0, rest.size() - ending.size()), base + 2 + static_cast<int>(ending.size()));
    }
  }
  // Past tense without a prefix: "رفتم" -> "رفتن", "خوردند" -> "خوردن".
  for (const auto& ending : kPersonalEndings) {
    if (!endsWith32(w, ending)) continue;
    const std::u32string stem = w.substr(0, w.size() - ending.size());
    // A past stem before a personal ending is read as a verb first: "کردیم" is
    // "کردن", not the word "کردی" with a possessive ending.
    if (letterCount(stem) >= 2 && (endsWith32(stem, U"د") || endsWith32(stem, U"ت"))) list.add(stem + U"ن", 0);
  }
  if (endsWith32(w, U"د") || endsWith32(w, U"ت")) list.add(w + U"ن", 3);
  // Participle and perfect: "رفته", "رفته‌ام" -> "رفتن".
  for (const std::u32string ending : {U"ه‌ام", U"ه‌ای", U"ه‌اید", U"ه‌اند", U"ه‌ایم", U"ه"}) {
    if (!endsWith32(w, ending)) continue;
    const std::u32string stem = w.substr(0, w.size() - ending.size());
    if (endsWith32(stem, U"د") || endsWith32(stem, U"ت")) list.add(stem + U"ن", 4 + static_cast<int>(ending.size()));
  }
}

}  // namespace

std::vector<std::string> english(const std::string& word) {
  std::string lower = word;
  std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return std::tolower(c); });
  std::vector<std::string> variants = englishCandidates(word);
  std::stable_sort(variants.begin(), variants.end(), [&lower](const std::string& a, const std::string& b) {
    return englishCost(lower, a) < englishCost(lower, b);
  });
  variants.erase(std::remove(variants.begin(), variants.end(), lower), variants.end());
  if (variants.size() > MAX_VARIANTS) variants.resize(MAX_VARIANTS);
  return variants;
}

bool isArabicScript(const std::string& word) {
  for (const char32_t cp : decode(word)) {
    if ((cp >= 0x0600 && cp <= 0x06FF) || (cp >= 0xFB50 && cp <= 0xFEFF)) return true;
  }
  return false;
}

std::vector<std::string> persian(const std::string& word) {
  const std::u32string original = decode(word);
  const std::u32string w = normalizePersian(original);
  PersianList list(original);
  list.add(w, 0);
  addPersianNouns(list, w);
  addPersianVerbs(list, w);
  // A compound written with a half-space may be a headword without it.
  if (w.find(ZWNJ) != std::u32string::npos) {
    std::u32string joined;
    for (const char32_t c : w)
      if (c != ZWNJ) joined.push_back(c);
    list.add(joined, 20);
  }
  return list.finish();
}

}  // namespace WordForms
