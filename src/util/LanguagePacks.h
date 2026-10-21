#pragma once

#include <I18n.h>

#include <string>

// VARAGH: menu languages left out of a small firmware (X3/X4) come from
// /languages/<code>.lng on the SD card: copied from the release zip by hand or
// downloaded in Settings > Language (LanguageDownloadActivity).
namespace LanguagePacks {

std::string path(Language lang);
std::string downloadUrl(Language lang);
bool installed(Language lang);
// Checks a pack file without keeping it (after a download).
bool valid(const std::string& filePath);
// Switches the menus to lang, loading its pack when it is not built in.
// Returns false (menus fall back to English) when the pack is missing or does
// not fit this firmware.
bool apply(uint8_t language);

}  // namespace LanguagePacks
