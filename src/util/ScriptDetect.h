#pragma once

#include <cctype>
#include <string_view>

// Cheap script checks used to pick a font that can draw a book before any
// layout happens. They look only at metadata strings, never at chapter text.
namespace ScriptDetect {

// True for BCP-47 / ISO 639 tags of languages written in Arabic script:
// "fa", "fa-IR", "ar", "per", "urd", ...
inline bool isArabicScriptLanguage(std::string_view tag) {
  size_t len = 0;
  while (len < tag.size() && std::isalpha(static_cast<unsigned char>(tag[len]))) ++len;
  if (len < 2 || len > 3) return false;
  char code[4] = {};
  for (size_t i = 0; i < len; ++i) code[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(tag[i])));
  const std::string_view c(code, len);
  for (const std::string_view arabicScript :
       {"ar", "fa", "ur", "ps", "sd", "ug", "ku", "ckb", "ara", "per", "fas", "urd", "pus", "snd", "uig"}) {
    if (c == arabicScript) return true;
  }
  return false;
}

// True when UTF-8 text contains an Arabic-block letter (U+0600-U+06FF, whose
// UTF-8 lead bytes are 0xD8-0xDB).
inline bool containsArabicScript(std::string_view utf8) {
  for (const char ch : utf8) {
    const auto b = static_cast<unsigned char>(ch);
    if (b >= 0xD8 && b <= 0xDB) return true;
  }
  return false;
}

}  // namespace ScriptDetect
