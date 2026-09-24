#include "GermanStemmer.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <initializer_list>

namespace GermanStemmer {
namespace {

// UTF-8 umlauts are two bytes: 0xC3 followed by one of these.
constexpr unsigned char UTF8_LEAD = 0xC3;
constexpr unsigned char LOWER_AE = 0xA4, LOWER_OE = 0xB6, LOWER_UE = 0xBC;
constexpr unsigned char UPPER_AE = 0x84, UPPER_OE = 0x96, UPPER_UE = 0x9C;

// Separable verb prefixes that put "ge"/"zu" inside the word:
// "aufgemacht" -> "aufmachen", "anzurufen" -> "anrufen".
constexpr const char* SEPARABLE_PREFIXES[] = {"zurück", "zusammen", "weiter", "nach", "fest", "fort", "heim", "vorbei",
                                              "weg",    "auf",      "aus",    "bei",  "ein",  "mit",  "vor",  "her",
                                              "hin",    "los",      "ab",     "an",   "zu"};

bool endsWith(const std::string& s, const char* suffix) {
  const size_t n = strlen(suffix);
  return s.size() >= n && s.compare(s.size() - n, n, suffix) == 0;
}

bool startsWith(const std::string& s, const char* prefix) {
  const size_t n = strlen(prefix);
  return s.size() >= n && s.compare(0, n, prefix) == 0;
}

// ASCII and Ä/Ö/Ü to lower case; other bytes pass through.
std::string toLower(const std::string& word) {
  std::string out = word;
  for (size_t i = 0; i < out.size(); ++i) {
    const auto c = static_cast<unsigned char>(out[i]);
    if (c >= 'A' && c <= 'Z') {
      out[i] = static_cast<char>(c + ('a' - 'A'));
    } else if (c == UTF8_LEAD && i + 1 < out.size()) {
      const auto next = static_cast<unsigned char>(out[i + 1]);
      if (next == UPPER_AE || next == UPPER_OE || next == UPPER_UE) out[i + 1] = static_cast<char>(next + 0x20);
      ++i;
    }
  }
  return out;
}

// Upper-case the first letter (ASCII or ä/ö/ü). Needed only for umlaut
// initials: the index comparison already folds ASCII case, but "ärger" and
// "Ärger" differ byte-for-byte.
std::string capitalizeUmlautInitial(const std::string& word) {
  if (word.size() < 2 || static_cast<unsigned char>(word[0]) != UTF8_LEAD) return {};
  const auto next = static_cast<unsigned char>(word[1]);
  if (next != LOWER_AE && next != LOWER_OE && next != LOWER_UE) return {};
  std::string out = word;
  out[1] = static_cast<char>(next - 0x20);
  return out;
}

// Replace the last ä/ö/ü with a/o/u: "häus" -> "haus", "bäum" -> "baum".
// Returns an empty string when the word has no umlaut.
std::string removeLastUmlaut(const std::string& word) {
  for (size_t i = word.size(); i >= 2; --i) {
    if (static_cast<unsigned char>(word[i - 2]) != UTF8_LEAD) continue;
    const auto c = static_cast<unsigned char>(word[i - 1]);
    char plain = 0;
    if (c == LOWER_AE) plain = 'a';
    if (c == LOWER_OE) plain = 'o';
    if (c == LOWER_UE) plain = 'u';
    if (!plain) continue;
    std::string out = word;
    out.replace(i - 2, 2, 1, plain);
    return out;
  }
  return {};
}

class VariantList {
 public:
  explicit VariantList(const std::string& original) : original(original) { list.reserve(MAX_VARIANTS); }

  void add(const std::string& candidate) {
    if (list.size() >= MAX_VARIANTS || candidate.size() < 3 || candidate == original) return;
    if (std::find(list.begin(), list.end(), candidate) != list.end()) return;
    list.push_back(candidate);
  }

  // A shortened stem plus its umlaut-free form ("häus" + "haus").
  void addStem(const std::string& stem) {
    add(stem);
    add(removeLastUmlaut(stem));
  }

  // A verb stem mapped to its infinitive: "mach" -> "machen", "wander" -> "wandern".
  void addInfinitive(const std::string& stem) {
    if (stem.size() < 2) return;
    if (endsWith(stem, "el") || endsWith(stem, "er")) add(stem + "n");
    add(stem + "en");
  }

  std::vector<std::string> take() { return std::move(list); }

 private:
  const std::string& original;
  std::vector<std::string> list;
};

std::string strip(const std::string& word, size_t n) { return word.size() > n ? word.substr(0, word.size() - n) : ""; }

// Past participle without a separable prefix: "gemacht" -> "machen",
// "gearbeitet" -> "arbeiten", "gefahren" -> "fahren".
void addParticiple(VariantList& out, const std::string& word, const std::string& prefix = {}) {
  if (!startsWith(word, "ge") || word.size() < 5) return;
  const std::string inner = word.substr(2);
  if (endsWith(inner, "et")) out.addInfinitive(prefix + strip(inner, 2));
  if (endsWith(inner, "t")) out.addInfinitive(prefix + strip(inner, 1));
  if (endsWith(inner, "en")) out.add(prefix + inner);
}

}  // namespace

std::vector<std::string> variants(const std::string& word) {
  const std::string w = toLower(word);
  VariantList out(word);

  // Case only: sentence-initial "Über" -> "über", and "ärger" -> "Ärger".
  out.add(w);
  out.add(capitalizeUmlautInitial(w));

  // Umlaut plural with no ending change: "Mütter" -> "Mutter", "Äpfel" -> "Apfel".
  out.add(removeLastUmlaut(w));

  // Feminine plural: "Lehrerinnen" -> "Lehrerin".
  if (endsWith(w, "innen")) out.add(strip(w, 3));

  // Comparative/superlative with adjective endings: "größeren" -> "groß",
  // "schönsten" -> "schön".
  for (const char* suffix : {"sten", "ster", "stem", "stes", "ste", "eren", "erem", "erer", "eres", "ere"}) {
    if (endsWith(w, suffix)) out.addStem(strip(w, strlen(suffix)));
  }

  // Noun plural/case and adjective endings, longest first.
  if (endsWith(w, "ern")) out.addStem(strip(w, 3));  // Kindern -> Kind
  for (const char* suffix : {"en", "em", "er", "es"}) {
    if (endsWith(w, suffix)) {
      out.addStem(strip(w, 2));  // Frauen -> Frau, Hauses -> Haus, Häuser -> Haus
      out.addStem(strip(w, 1));  // Blumen -> Blume
    }
  }
  if (endsWith(w, "e") || endsWith(w, "n") || endsWith(w, "s")) out.addStem(strip(w, 1));  // Hunde, Autos

  // Weak-verb past tense: "machte" / "arbeitete" -> infinitive.
  for (const char* suffix : {"etest", "eten", "etet", "ete", "test", "ten", "tet", "te"}) {
    if (endsWith(w, suffix)) {
      out.addInfinitive(strip(w, strlen(suffix)));
      break;
    }
  }

  // Present tense: "machst" / "macht" / "arbeitet" / "mache" -> infinitive.
  for (const char* suffix : {"est", "et", "st", "t", "e"}) {
    if (endsWith(w, suffix)) {
      out.addInfinitive(strip(w, strlen(suffix)));
      break;
    }
  }

  // Participles, also when used as adjectives ("die gemachte Arbeit").
  addParticiple(out, w);
  for (const char* suffix : {"en", "em", "er", "es", "e"}) {
    if (endsWith(w, suffix)) {
      addParticiple(out, strip(w, strlen(suffix)));
      break;
    }
  }

  // Separable verbs: "aufgemacht" -> "aufmachen", "anzurufen" -> "anrufen".
  for (const char* prefix : SEPARABLE_PREFIXES) {
    if (!startsWith(w, prefix)) continue;
    const std::string rest = w.substr(strlen(prefix));
    addParticiple(out, rest, prefix);
    if (startsWith(rest, "zu") && endsWith(rest, "en") && rest.size() > 5) out.add(prefix + rest.substr(2));
  }

  return out.take();
}

bool isGermanSource(const char* lang, const char* bookname, const char* folderPath) {
  if (lang && lang[0]) {
    const std::string l = toLower(lang);
    return startsWith(l, "de") && (l.size() == 2 || !isalpha(static_cast<unsigned char>(l[2])));
  }

  std::string haystack = toLower(bookname ? bookname : "");
  haystack += ' ';
  haystack += toLower(folderPath ? folderPath : "");

  for (const char* code : {"de-en", "de_en", "deu-eng", "deu_eng", "ger-eng"}) {
    if (haystack.find(code) != std::string::npos) return true;
  }
  for (const char* code : {"en-de", "en_de", "eng-deu", "eng_deu", "eng-ger"}) {
    if (haystack.find(code) != std::string::npos) return false;
  }

  // "German-English" is German-source; "English-German" is not.
  const size_t german = std::min(haystack.find("german"), haystack.find("deutsch"));
  if (german == std::string::npos) return false;
  const size_t english = std::min(haystack.find("english"), haystack.find("englisch"));
  return english == std::string::npos || german < english;
}

}  // namespace GermanStemmer
