#pragma once

#include <I18n.h>

#include "activities/Activity.h"
#include "activities/ScreenTransitionRefresh.h"

// VARAGH: downloads one menu-language pack (util/LanguagePacks) over Wi-Fi.
// The caller checks LanguagePacks::installed afterwards and switches to it.
class LanguageDownloadActivity final : public Activity {
 public:
  LanguageDownloadActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, Language lang)
      : Activity("LanguageDownload", renderer, mappedInput), lang(lang) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  bool skipLoopDelay() override { return true; }
  void render(RenderLock&&) override;

 private:
  enum State { DOWNLOADING, FAILED };
  State state = DOWNLOADING;
  Language lang;
  bool started = false;
  bool shouldTearDownWifiOnExit = false;
  ScreenTransitionRefresh screenTransitionRefresh;

  void download();
};
