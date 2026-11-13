#pragma once

#include "activities/Activity.h"
#include "activities/ScreenTransitionRefresh.h"

// VARAGH: downloads the card files this firmware no longer carries in flash (the Bitter font and the
// hyphenation tables of six languages) over Wi-Fi. They sit in one release shared by every version
// (EXTRA_FILES_RELEASE), so a new firmware needs no new upload unless a file format changes.
class ExtraFilesDownloadActivity final : public Activity {
 public:
  ExtraFilesDownloadActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("ExtraFilesDownload", renderer, mappedInput) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  bool skipLoopDelay() override { return true; }
  void render(RenderLock&&) override;

 private:
  enum State { DOWNLOADING, DONE, FAILED };
  State state = DOWNLOADING;
  uint8_t fileNumber = 0;
  bool started = false;
  bool shouldTearDownWifiOnExit = false;
  ScreenTransitionRefresh screenTransitionRefresh;

  void download();
};
