#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

// Text side of the online dictionary: which site to ask for a word, and how to
// turn the answer into an entry laid out like the VARAGH dictionaries. Pure
// string work, no network and no storage, so it is tested on the host.
namespace onlinedict {

// The reading languages VARAGH offers (Settings > Reader > Dictionaries).
// Values are persisted (bit masks, request file): append only.
enum class Lang : uint8_t {
  English = 0,
  German = 1,
  Persian = 2,
  French = 3,
  Spanish = 4,
  Italian = 5,
  Dutch = 6,
  Portuguese = 7,
  Russian = 8,
  Arabic = 9,
  Turkish = 10,
  Polish = 11,
};
constexpr uint8_t kLangCount = 12;
// The language's name in itself ("Deutsch", "فارسی").
const char* nativeName(Lang lang);

// "en", "de", "fa": the Wikimedia language edition and the name of the saved-answers file.
const char* code(Lang lang);
bool langFromCode(const char* text, Lang& lang);

// A word in Arabic letters is Arabic when the dictionary in use is Arabic,
// Persian otherwise; any other word has the language of the dictionary in use
// (the lang= of its .ifo, "de-de" is German), and English when that says nothing.
Lang languageFor(const std::string& word, const char* dictionaryLang);

// Trims the word, drops Arabic vowel marks and tatweel, and writes the Arabic
// forms of kaf and yeh as the Persian letters the Persian sites use.
std::string normalizeWord(const std::string& word);

// The word with the case of its first ASCII letter switched ("Serendipity" at
// the start of a sentence is "serendipity" on Wiktionary). Empty when the word
// does not start with an ASCII letter.
std::string otherCase(const std::string& word);

std::string percentEncode(const std::string& text);
// Wiktionary. German and Persian can ask about a second spelling in the same
// request (see Extractor); the English address takes one word only.
std::string definitionUrl(Lang lang, const std::string& word, const std::string& otherSpelling = {});
std::string summaryUrl(Lang lang, const std::string& word);  // Wikipedia
// English Wiktionary's REST answer covers every language; the Extractor keeps
// the block of the word's own language, so German and Persian words get
// English meanings from it.
std::string englishWiktionaryUrl(const std::string& word);
std::string datamuseUrl(const std::string& word);       // English, WordNet meanings
std::string openThesaurusUrl(const std::string& word);  // German synonyms

// VARAGH 1.3.0: the online sources the reader can choose per language
// (Settings > Reader). Values are persisted: append only.
enum class Provider : uint8_t {
  Wiktionary = 0,         // the word's own Wiktionary (Wikipedia when it has none)
  EnglishWiktionary = 1,  // English meanings of a German or Persian word
  Datamuse = 2,           // English only
  OpenThesaurus = 3,      // German only, synonyms
  Wikipedia = 4,
  None = 5,               // no second source
};
constexpr uint8_t kProviderCount = 5;  // real sources, None not counted

const char* providerName(Provider provider);
// Short name used in the file names of the saved answers ("" for Wiktionary,
// whose answers keep the file names from before 1.3.0).
const char* providerFileTag(Provider provider);
bool providerServes(Provider provider, Lang lang);
// The source after `current` that serves the language, wrapping around.
Provider nextProvider(Provider current, Lang lang);

enum class Source : uint8_t {
  // en.wiktionary REST "definition" answer: parts of speech with their senses.
  WiktionaryDefinitions,
  // MediaWiki "extracts" answer: the entry page as plain text (German, Persian).
  WiktionaryExtract,
  // Wikipedia REST "summary" answer.
  WikipediaSummary,
  // Datamuse "words" answer with md=d: WordNet meanings, "n\tmeaning".
  DatamuseDefinitions,
  // OpenThesaurus search answer: groups of synonyms.
  OpenThesaurusSynonyms,
};

// Reads an answer as it arrives, keeping only the few strings an entry needs,
// so an answer of any size costs a bounded amount of memory.
//
// A WiktionaryExtract answer may cover two spellings of the word (the same word
// with its first letter in the other case). The site then sends the text of
// only one of the pages it has, the capitalised one first, so:
// - the entry under `word` itself is always preferred;
// - the entry under `otherSpelling` is taken when the site has none under `word`;
// - when the site has both but sent only the other one's text, finish() is false
//   and askAgain() is true: `word` has to be asked for again on its own.
class Extractor {
 public:
  Extractor(Source source, Lang lang, std::string word, std::string otherSpelling = {});

  void feed(const char* data, size_t length);
  // True when the answer held an entry; html then has it. Call once, after the last feed().
  bool finish(std::string& html);
  // After finish(): the spelling the entry was found under.
  const std::string& foundWord() const { return foundOther_ ? other_ : word_; }
  // After finish() returned false, see above.
  bool askAgain() const { return askAgain_; }

 private:
  void onKey();
  void onString();
  void appendCodepoint(uint32_t codepoint);
  void closeSection();

  const Source source_;
  const Lang lang_;
  const std::string word_;
  const std::string other_;

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
  bool unusable_ = false;    // disambiguation page or error answer
  // WiktionaryExtract: the page being read, and what is known of the two spellings.
  std::string pageTitle_;
  std::string otherExtract_;  // text of a page that is not the word itself
  std::string otherTitle_;
  bool wordHasText_ = false;
  bool wordPageSeen_ = false;
  bool wordPageMissing_ = false;
  bool foundOther_ = false;
  bool askAgain_ = false;
  int sections_ = 0;
  int senses_ = 0;
  int examples_ = 0;
  bool sectionOpen_ = false;
  bool exampleForSense_ = false;
  // Datamuse / OpenThesaurus.
  bool wordMatches_ = false;
  std::string lastPos_;
  std::string group_;  // OpenThesaurus: the synonyms of the current group
  int groups_ = 0;
  void closeGroup();
};

}  // namespace onlinedict
