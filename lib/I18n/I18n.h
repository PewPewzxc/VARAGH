#pragma once

#include <cstdint>

#include "I18nKeys.h"
/**
 * Internationalization (i18n) system for CrossPoint Reader
 */

class I18n {
 public:
  static I18n& getInstance();

  // Disable copy
  I18n(const I18n&) = delete;
  I18n& operator=(const I18n&) = delete;

  // Get localized string by ID
  const char* get(StrId id) const;

  const char* operator[](StrId id) const { return get(id); }

  Language getLanguage() const { return _language; }
  // A language that is not built in falls back to English unless its pack was
  // handed over first with usePack.
  void setLanguage(Language lang);
  static bool isBuiltin(Language lang);
  // VARAGH: strings of a language left out of the build, read from an SD-card
  // pack (src/util/LanguagePacks). The memory stays the caller's; pass nullptr
  // to drop it.
  void usePack(Language lang, const char* data, const uint16_t* offsets);
  const char* getLanguageName(Language lang) const;
  static Language languageFromCode(const char* code);

  // Get all unique characters used in a specific language
  // Returns a sorted string of unique characters
  static const char* getCharacterSet(Language lang);

 private:
  I18n() : _language(Language::EN) {}

  Language _language;
  Language _packLanguage = Language::EN;
  const char* _packData = nullptr;
  const uint16_t* _packOffsets = nullptr;
};

// Convenience macros
#define tr(id) I18n::getInstance().get(StrId::id)
#define I18N I18n::getInstance()
