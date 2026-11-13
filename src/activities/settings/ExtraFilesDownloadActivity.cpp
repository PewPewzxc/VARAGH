#include "ExtraFilesDownloadActivity.h"

#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>
#include <Logging.h>
#include <SdCardFontRegistry.h>
#include <WiFi.h>

#include <cstdio>
#include <iterator>
#include <string>

#include "MappedInputManager.h"
#include "SdCardFontSystem.h"
#include "activities/network/WifiSelectionActivity.h"
#include "components/TouchHeaderBackButton.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "network/HttpDownloader.h"

namespace {

// Bump the number only when a file's format changes; firmware versions share the release.
constexpr char EXTRA_FILES_RELEASE[] = "https://github.com/PewPewzxc/VARAGH/releases/download/card-files-1/";
constexpr char HYPHENATION_DIR[] = "/.crosspoint/hyphenation";

struct ExtraFile {
  const char* name;
  bool font;  // Bitter face (fonts folder) or hyphenation table
};
constexpr ExtraFile FILES[] = {
    {"bitter_regular.ttf", true}, {"bitter_bold.ttf", true}, {"bitter_italic.ttf", true},
    {"bitter_bolditalic.ttf", true}, {"hyph-fr.bin", false}, {"hyph-ru.bin", false},
    {"hyph-es.bin", false}, {"hyph-sv.bin", false}, {"hyph-uk.bin", false},
    {"hyph-pl.bin", false},
};

}  // namespace

void ExtraFilesDownloadActivity::onEnter() {
  Activity::onEnter();
  state = DOWNLOADING;
  fileNumber = 0;
  started = WiFi.status() == WL_CONNECTED;
  if (started) {
    requestUpdate();
    return;
  }
  shouldTearDownWifiOnExit = true;
  WiFi.mode(WIFI_STA);
  startActivityForResult(std::make_unique<WifiSelectionActivity>(renderer, mappedInput),
                         [this](const ActivityResult& result) {
                           if (result.isCancelled) {
                             finish();
                             return;
                           }
                           started = true;
                           requestUpdate();
                         });
}

void ExtraFilesDownloadActivity::onExit() {
  Activity::onExit();
  if (shouldTearDownWifiOnExit && WiFi.getMode() != WIFI_MODE_NULL) {
    WiFi.disconnect(false);
    delay(30);
    WiFi.mode(WIFI_OFF);
  }
}

void ExtraFilesDownloadActivity::download() {
  // An existing Bitter folder keeps its place; a new one goes where new fonts go.
  const char* fontRoot = SdCardFontRegistry::findFamilyRoot("Bitter");
  if (!fontRoot) fontRoot = SdCardFontRegistry::defaultWriteRoot();
  const std::string fontDir = std::string(fontRoot) + "/Bitter";
  Storage.mkdir(fontRoot);
  Storage.mkdir(fontDir.c_str());
  Storage.mkdir("/.crosspoint");
  Storage.mkdir(HYPHENATION_DIR);

  bool ok = true;
  for (const ExtraFile& file : FILES) {
    ++fileNumber;
    const std::string dest = (file.font ? fontDir : std::string(HYPHENATION_DIR)) + "/" + file.name;
    if (Storage.exists(dest.c_str())) continue;  // already on the card: nothing to fetch
    requestUpdateAndWait();
    const std::string temp = dest + ".part";
    Storage.remove(temp.c_str());
    const auto result = HttpDownloader::downloadToFile(std::string(EXTRA_FILES_RELEASE) + file.name, temp);
    // A cut-off file is never installed: the font or table loader would trip over it.
    if (result != HttpDownloader::OK || !Storage.rename(temp.c_str(), dest.c_str())) {
      LOG_ERR("XFILES", "Download failed: %s (%d)", file.name, static_cast<int>(result));
      Storage.remove(temp.c_str());
      ok = false;
      break;
    }
  }
  sdFontSystem.markRegistryDirty();  // Bitter shows up in Font Family without a restart
  state = ok ? DONE : FAILED;
  requestUpdate();
}

void ExtraFilesDownloadActivity::loop() {
  if (state == DOWNLOADING) {
    if (!started) return;
    download();
    return;
  }
  int x = 0;
  int y = 0;
  if (TouchHeaderBackButton::wasTapped(mappedInput, renderer) ||
      mappedInput.wasPressed(MappedInputManager::Button::Back) || mappedInput.wasScreenTapped(x, y)) {
    finish();
  }
}

void ExtraFilesDownloadActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const Rect header = TouchHeaderBackButton::headerRect(renderer, mappedInput);
  if (mappedInput.hasTouchHardware()) {
    TouchHeaderBackButton::draw(renderer, header, tr(STR_EXTRA_FILES), false);
  } else {
    GUI.drawHeader(renderer, header, tr(STR_EXTRA_FILES));
  }
  const int midY = renderer.getScreenHeight() / 2;
  if (state == DOWNLOADING) {
    char progress[48];
    std::snprintf(progress, sizeof(progress), "%s %u / %u", tr(STR_DOWNLOADING), static_cast<unsigned>(fileNumber),
                  static_cast<unsigned>(std::size(FILES)));
    renderer.drawCenteredText(UI_12_FONT_ID, midY, fileNumber == 0 ? tr(STR_DOWNLOADING) : progress);
  } else {
    renderer.drawCenteredText(UI_12_FONT_ID, midY - 20, state == DONE ? tr(STR_DONE) : tr(STR_DOWNLOAD_FAILED), true,
                              EpdFontFamily::BOLD);
    renderer.drawCenteredText(UI_10_FONT_ID, midY + 10,
                              state == DONE ? tr(STR_EXTRA_FILES_DESC) : tr(STR_EXTRA_FILES_COPY_HINT));
    const auto labels = mappedInput.mapLabels(mappedInput.withBackArrow(tr(STR_BACK)), "", "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  }
  renderer.displayBuffer(screenTransitionRefresh.modeFor(static_cast<uint8_t>(state)));
}
