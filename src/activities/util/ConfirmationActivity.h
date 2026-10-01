#pragma once
#include <functional>
#include <string>

#include "activities/Activity.h"
#include "components/OptionPopup.h"

class ConfirmationActivity : public Activity {
 private:
  std::string popupTitle;
  OptionPopup confirmPopup;
  bool ignoreConfirmRelease = false;
  bool overrideDisabledReaderTouchscreen = false;
  // When non-zero, the dialog cancels itself this long after it opens.
  uint32_t autoCancelMs = 0;
  unsigned long enteredAtMs = 0;

 public:
  ConfirmationActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, const std::string& heading,
                       const std::string& body, bool ignoreInitialConfirmRelease = false,
                       bool overrideDisabledReaderTouchscreen = false, uint32_t autoCancelMs = 0);

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&& lock) override;
  bool allowPowerAsConfirmInReaderMode() const override { return true; }
};
