#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

// Text side of the online dictionary: which site to ask for a word, and how to
// turn the answer into an entry laid out like the VARAGH dictionaries. Pure
// string work, no network and no storage, so it is tested on the host.
namespace onlinedict {

enum class Lang : uint8_t { English = 0, German = 1, Persian = 2 };

// "en", "de", "fa": the Wikimedia language edition and the name of the saved-answers file.
const char* code(Lang lang);
bool langFromCode(const char* text, Lang& lang);

// Persian when the word holds Arabic-script letters; otherwise the language of
// the headwords of the dictionary in use (the lang= of its .ifo, "de-de" is
// German), and English when that says nothing.
Lang languageFor(const std::string& word, const char* dictionaryLang);

// Trims the word, drops Arabic vowel marks and tatweel, and writes the Arabic
// forms of kaf and yeh as the Persian letters the Persian sites use.
std::string normalizeWord(const std::string& word);

// The word with the case of its first ASCII letter switched ("Serendipity" at
// the start of a sentence is "serendipity" on Wiktionary). Empty when the word
// does not start with an ASCII letter.
std::string otherCase(const std::string& word);

std::string percentEncode(const std::string& text);
std::string definitionUrl(Lang lang, const std::string& word);  // Wiktionary
std::string summaryUrl(Lang lang, const std::string& word);     // Wikipedia
// A small request that is answered whenever the site can be reached, to tell
// "no such entry" from "no connection".
std::string probeUrl(Lang lang);

enum class Source : uint8_t {
  // en.wiktionary REST "definition" answer: parts of speech with their senses.
  WiktionaryDefinitions,
  // MediaWiki "extracts" answer: the entry page as plain text (German, Persian).
  WiktionaryExtract,
  // Wikipedia REST "summary" answer.
  WikipediaSummary,
};

// Reads an answer as it arrives, keeping only the few strings an entry needs,
// so an answer of any size costs a bounded amount of memory.
class Extractor {
 public:
  Extractor(Source source, Lang lang, std::string word);

  void feed(const char* data, size_t length);
  // True when the answer held an entry; html then has it. Call once, after the last feed().
  bool finish(std::string& html);

 private:
  void onKey();
  void onString();
  void appendCodepoint(uint32_t codepoint);
  void closeSection();

  const Source source_;
  const Lang lang_;
  const std::string word_;

  // JSON reading state.
  int depth_ = 0;
  bool inString_ = false;
  bool escaped_ = false;
  bool stringPending_ = false;  // a string has ended; the next character says whether it was a key
  int unicodeDigits_ = 0;
  uint32_t unicodeValue_ = 0;
  uint32_t highSurrogate_ = 0;
  std::string token_;
  std::string key_;
  size_t tokenLimit_ = 0;

  // What has been gathered.
  std::string html_;
  std::string topKey_;       // WiktionaryDefinitions: language key of the current block
  std::string extract_;      // page text or summary
  std::string title_;
  std::string description_;
  bool unusable_ = false;    // missing page, disambiguation page or error answer
  int sections_ = 0;
  int senses_ = 0;
  int examples_ = 0;
  bool sectionOpen_ = false;
  bool exampleForSense_ = false;
};

}  // namespace onlinedict
