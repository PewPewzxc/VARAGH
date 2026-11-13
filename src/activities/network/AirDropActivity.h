#pragma once
#include <sdkconfig.h>
#if CROSSPOINT_AIRDROP
#include <BoardConfig.h>
#if !(FREEINK_DEVICE_X4PRO || FREEINK_DEVICE_X4CLASSIC) || !FREEINK_CAP_USB_MSC || !FREEINK_FB_PSRAM
#error "AirDrop requires an X4 Pro or X4 Classic profile (ESP32-S3 with PSRAM)"
#endif
static_assert(CONFIG_IDF_TARGET_ESP32S3, "AirDrop requires ESP32-S3");
#include <ESP32Drop.h>
#include <HalStorage.h>
#include <airdrop/core/ad_cpio_stream.h>

#include <string>
#include <vector>

#include "activities/Activity.h"

// VARAGH: AirDrop receiver from crosspoint-reader PR 4314, opened only after a network boot
// (clean heap) and left by a silent restart, like Lion's other radio screens.
class AirDropActivity final : public Activity {
 public:
  AirDropActivity(GfxRenderer& renderer, MappedInputManager& input) : Activity("AirDrop", renderer, input) {}
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool preventAutoSleep() override { return true; }
  bool skipLoopDelay() override { return true; }
  // The panel would stop the polling that serves consent, storage and the session deadline.
  bool allowFrontlightPanelGesture() const override { return false; }

 private:
  enum class State { Waiting, Asking, Receiving, Saved, Declined, TooLarge, Error };
  enum class Action : uint8_t { Accept, Decline, Read, ShowFiles, Stop, Ok };
  static constexpr uint32_t RECEIVE_LIMIT = 30u * 1024u * 1024u;
  static constexpr uint32_t SESSION_MS = 10u * 60u * 1000u;
  static constexpr uint32_t MAX_FILES = 200;

  State state = State::Waiting;
  bool radioStarted = false, receiverStarted = false, leaving = false;
  bool drawn = false, finished = false;  // finished: archive saved, photos not yet converted
  uint32_t startedAt = 0, lastStats = 0, lastProgressDraw = 0, observedErrors = 0, observedLimitRejections = 0;
  AdStatus stats{};
  char savedPath[288]{}, temporaryPath[300]{}, askName[128]{};
  uint32_t askFiles = 0, askBytes = 0, drawnBytes = 0;
  std::vector<std::string> photos;  // JPEGs of this transfer, converted after the reply
  volatile uint32_t receivedBytes = 0;  // written by the stream callback, read by render

  AdCpioStream archive;
  HalFile output;
  bool temporaryOwned = false;
  uint32_t expectedSize = 0, writtenSize = 0, filesThisTransfer = 0;

  void answerRequest(bool accept);
  uint8_t actions(const char** labels, Action* acts) const;
  void run(Action action);
  void setState(State next);
  void convertPhotos();
  void stop();
  void leave(bool showFiles);
  bool beginFile(const char* name, uint32_t len);
  bool writeFile(const uint8_t* data, uint32_t len);
  bool endFile();
  void abortFile();
  static bool receiveStream(void* ctx, AdStreamEvent event, const uint8_t* data, uint32_t len);
};
#endif
