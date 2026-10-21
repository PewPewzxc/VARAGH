#include "LanguageDownloadActivity.h"

#include <GfxRenderer.h>
#include <HalStorage.h>
#include <Logging.h>
#include <WiFi.h>

#include "MappedInputManager.h"
#include "activities/network/WifiSelectionActivity.h"
#include "components/TouchHeaderBackButton.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "network/HttpDownloader.h"
#include "util/LanguagePacks.h"

void LanguageDownloadActivity::onEnter() {
  Activity::onEnter();
  state = DOWNLOADING;
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

void LanguageDownloadActivity::onExit() {
  Activity::onExit();
  if (shouldTearDownWifiOnExit && WiFi.getMode() != WIFI_MODE_NULL) {
    WiFi.disconnect(false);
    delay(30);
    WiFi.mode(WIFI_OFF);
  }
}

void LanguageDownloadActivity::download() {
  const std::string dest = LanguagePacks::path(lang);
  const std::string temp = dest + ".part";
  Storage.mkdir("/languages");
  Storage.remove(temp.c_str());
  const auto result = HttpDownloader::downloadToFile(LanguagePacks::downloadUrl(lang), temp);
  // A pack for another firmware version or a cut-off file is never installed.
  if (result == HttpDownloader::OK && LanguagePacks::valid(temp)) {
    Storage.remove(dest.c_str());
    if (Storage.rename(temp.c_str(), dest.c_str())) {
      finish();
      return;
    }
  }
  LOG_ERR("LANG", "Language pack download failed (%d)", static_cast<int>(result));
  Storage.remove(temp.c_str());
  state = FAILED;
  requestUpdate();
}

void LanguageDownloadActivity::loop() {
  if (state == DOWNLOADING) {
    if (!started) return;
    // Show "Downloading..." before the blocking download.
    requestUpdateAndWait();
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

void LanguageDownloadActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const Rect header = TouchHeaderBackButton::headerRect(renderer, mappedInput);
  const char* title = I18N.getLanguageName(lang);
  if (mappedInput.hasTouchHardware()) {
    TouchHeaderBackButton::draw(renderer, header, title, false);
  } else {
    GUI.drawHeader(renderer, header, title);
  }
  const int midY = renderer.getScreenHeight() / 2;
  if (state == DOWNLOADING) {
    renderer.drawCenteredText(UI_12_FONT_ID, midY, tr(STR_DOWNLOADING));
  } else {
    renderer.drawCenteredText(UI_12_FONT_ID, midY - 20, tr(STR_DOWNLOAD_FAILED), true, EpdFontFamily::BOLD);
    renderer.drawCenteredText(UI_10_FONT_ID, midY + 10, tr(STR_LANGUAGE_PACK_HINT));
    const auto labels = mappedInput.mapLabels(mappedInput.withBackArrow(tr(STR_BACK)), "", "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  }
  renderer.displayBuffer(screenTransitionRefresh.modeFor(static_cast<uint8_t>(state)));
}
