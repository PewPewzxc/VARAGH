#pragma once

#include <string>
#include <vector>

// Candidate base forms for a German word, for dictionary lookup only.
//
// This is not a linguistic stemmer: it proposes a short, ordered list of
// plausible headwords (plural/case endings removed, verb forms mapped back to
// an infinitive, umlaut plurals mapped to the singular vowel) and lets the
// dictionary index act as the filter. The first candidate found wins, so the
// most likely forms come first. Irregular forms ("ging", "sah") are left to the
// dictionary's .syn alternate-form file.
namespace GermanStemmer {

// Upper bound on returned candidates. Each candidate costs one index probe on
// the SD card, so the list stays short even for long compound words.
constexpr size_t MAX_VARIANTS = 24;

// `word` is UTF-8 as selected in the reader. The result never contains `word`
// itself and never contains duplicates.
std::vector<std::string> variants(const std::string& word);

// True when the active dictionary translates *from* German, judged from the
// .ifo `lang` field (e.g. "de-en") when present, otherwise from the dictionary
// name/folder ("German-English", "de-en", "Deutsch-Englisch").
bool isGermanSource(const char* lang, const char* bookname, const char* folderPath);

}  // namespace GermanStemmer
