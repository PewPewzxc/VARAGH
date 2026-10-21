#include "LanguagePacks.h"

#include <HalStorage.h>
#include <Logging.h>

#include <cctype>
#include <memory>
#include <new>

#include "util/LanguagePackFormat.h"

#ifndef VARAGH_RELEASE_TAG
#define VARAGH_RELEASE_TAG CROSSINK_VERSION
#endif

namespace LanguagePacks {
namespace {

constexpr char kDir[] = "/languages";
// Larger than any pack (Persian is ~35 KB); a bigger file is not a pack.
constexpr size_t kMaxPackSize = 64 * 1024;

// The loaded pack. uint16_t storage keeps the offsets aligned.
std::unique_ptr<uint16_t[]> packBuffer;

std::string lowerCode(const Language lang) {
  std::string code = LANGUAGE_CODES[static_cast<size_t>(lang)];
  for (char& c : code) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return code;
}

// Reads and checks a pack; on success buffer holds it and data/offsets point into it.
bool readPack(const std::string& filePath, std::unique_ptr<uint16_t[]>& buffer, const char*& data,
              const uint16_t*& offsets) {
  HalFile file;
  if (!Storage.openFileForRead("LANG", filePath.c_str(), file)) return false;
  const size_t size = file.fileSize();
  if (size == 0 || size > kMaxPackSize) {
    file.close();
    return false;
  }
  buffer.reset(new (std::nothrow) uint16_t[(size + 1) / 2]);
  if (!buffer) {
    file.close();
    LOG_ERR("LANG", "No memory for %u-byte pack", static_cast<unsigned>(size));
    return false;
  }
  auto* bytes = reinterpret_cast<uint8_t*>(buffer.get());
  const int read = file.read(bytes, size);
  file.close();
  if (read != static_cast<int>(size) ||
      !LanguagePackFormat::parse(bytes, size, I18N_LAYOUT_ID, static_cast<uint16_t>(StrId::_COUNT), data,
                                 offsets)) {
    LOG_ERR("LANG", "Pack %s does not fit this firmware", filePath.c_str());
    buffer.reset();
    return false;
  }
  return true;
}

}  // namespace

std::string path(const Language lang) { return std::string(kDir) + "/" + lowerCode(lang) + ".lng"; }

std::string downloadUrl(const Language lang) {
  return std::string("https://github.com/PewPewzxc/VARAGH/releases/download/") + VARAGH_RELEASE_TAG + "/" +
         CROSSINK_FIRMWARE_DEVICE_TYPE + "-lang-" + lowerCode(lang) + ".lng";
}

bool installed(const Language lang) { return Storage.exists(path(lang).c_str()); }

bool valid(const std::string& filePath) {
  std::unique_ptr<uint16_t[]> buffer;
  const char* data = nullptr;
  const uint16_t* offsets = nullptr;
  return readPack(filePath, buffer, data, offsets);
}

bool apply(const uint8_t language) {
  const auto lang = static_cast<Language>(language < getLanguageCount() ? language : 0);
  if (I18n::isBuiltin(lang)) {
    I18N.setLanguage(lang);
    I18N.usePack(Language::EN, nullptr, nullptr);
    packBuffer.reset();
    return true;
  }
  std::unique_ptr<uint16_t[]> buffer;
  const char* data = nullptr;
  const uint16_t* offsets = nullptr;
  if (!readPack(path(lang), buffer, data, offsets)) {
    I18N.setLanguage(Language::EN);
    I18N.usePack(Language::EN, nullptr, nullptr);
    packBuffer.reset();
    return false;
  }
  I18N.usePack(lang, data, offsets);
  I18N.setLanguage(lang);
  packBuffer = std::move(buffer);  // the old pack is freed only after I18n points at the new one
  return true;
}

}  // namespace LanguagePacks
