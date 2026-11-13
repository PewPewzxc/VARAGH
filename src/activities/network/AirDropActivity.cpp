#include "AirDropActivity.h"
#if CROSSPOINT_AIRDROP
#include <FontCacheManager.h>
#include <FsHelpers.h>
#include <GfxRenderer.h>
#include <HalPowerManager.h>
#include <I18n.h>
#include <JpegToBmpConverter.h>
#include <WiFi.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "CrossPointState.h"
#include "MappedInputManager.h"
#include "SilentRestart.h"
#include "activities/ActivityManager.h"
#include "components/CompactHeader.h"
#include "components/TouchActionButtons.h"
#include "components/TouchHeaderBackButton.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
constexpr char RECEIVE_DIR[] = "/AirDrop";
constexpr uint32_t DECLINED_SHOW_MS = 5000;
constexpr uint32_t PROGRESS_DRAW_MS = 1000;
constexpr char LOG_PATH[] = "/.crosspoint/airdrop-log.txt";
constexpr size_t LOG_MAX = 64 * 1024;
// shortcut: photos past this count stay JPEG only; raise it if people send albums.
constexpr size_t MAX_PHOTOS = 20;

// Moves the library's diagnostic lines to the card (main task only: it owns storage).
void saveLog() {
  char buf[512];
  uint32_t n = ad_log_take(buf, sizeof(buf));
  if (!n) return;
  HalFile file = Storage.open(LOG_PATH, O_WRONLY | O_CREAT | O_APPEND);
  if (file && file.size() > LOG_MAX) {
    file.close();
    Storage.remove(LOG_PATH);
    file = Storage.open(LOG_PATH, O_WRONLY | O_CREAT | O_APPEND);
  }
  if (!file) return;
  do {
    file.write(reinterpret_cast<const uint8_t*>(buf), n);
  } while ((n = ad_log_take(buf, sizeof(buf))));
  file.close();
}

bool readable(const std::string_view path) {
  return FsHelpers::hasEpubExtension(path) || FsHelpers::hasXtcExtension(path) || FsHelpers::hasTxtExtension(path);
}
// What the file list shows; everything else is saved but invisible there.
bool listed(const std::string_view path) {
  return readable(path) || FsHelpers::hasMarkdownExtension(path) || FsHelpers::hasBmpExtension(path) ||
         FsHelpers::hasPngExtension(path);
}

void formatMb(char* out, const size_t size, const uint32_t bytes) {
  const uint64_t tenths = (static_cast<uint64_t>(bytes) * 10 + 524288) / 1048576;  // 0.1 MB steps, rounded
  snprintf(out, size, "%lu.%lu MB", static_cast<unsigned long>(tenths / 10), static_cast<unsigned long>(tenths % 10));
}

// Same placement as Lion's Receive File screen.
TouchActionButtons::Layout actionLayout(const GfxRenderer& renderer, const uint8_t count) {
  const Rect screen = UITheme::getInstance().getScreenSafeArea(renderer, true, false);
  constexpr int sideMargin = 24;
  constexpr int bottomMargin = 12;
  const int total = TouchActionButtons::kDefaultHeight * count + TouchActionButtons::kDefaultGap * (count - 1);
  return TouchActionButtons::vertical(
      Rect{screen.x + sideMargin, screen.y + screen.height - bottomMargin - total, screen.width - sideMargin * 2, total},
      count);
}
}  // namespace

void AirDropActivity::onEnter() {
  Activity::onEnter();
  if (auto* cache = renderer.getFontCacheManager()) cache->releaseSdFontCaches();
  WiFi.mode(WIFI_OFF);
  // AWDL keeps the phone's availability windows; a lowered CPU clock misses them.
  powerManager.setPowerSaving(false);
  startedAt = millis();
  LOG_INF("AIRDROP", "Start heap=%u largest=%u", ESP.getFreeHeap(), ESP.getMaxAllocHeap());
  const bool ready = Storage.ensureDirectoryExists(RECEIVE_DIR) && ad_prepare_tls();
  ad_logf("=== session %s ready=%d heap=%u largest=%u", ad_receiver_name(), ready, ESP.getFreeHeap(),
          ESP.getMaxAllocHeap());
  if (!ready) {
    saveLog();
    setState(State::Error);
    requestUpdate();
    return;
  }
  radioStarted = awdl_begin() == ESP_OK;
  if (!radioStarted) {
    LOG_ERR("AIRDROP", "Radio start failed; restarting to release it");
    ad_logf("radio start failed");
    saveLog();
    silentRestart();
    return;
  }
  ad_on_stream(&AirDropActivity::receiveStream, this);
  receiverStarted = ad_begin(RECEIVE_LIMIT) == 0;
  ad_logf("receiver started=%d heap=%u", receiverStarted, ESP.getFreeHeap());
  saveLog();
  if (!receiverStarted) {
    setState(State::Error);
  } else {
    ad_stats_read(&stats);
    observedErrors = stats.file_err;
    observedLimitRejections = stats.limit_rejections;
  }
  requestUpdate();
}

void AirDropActivity::stop() {
  if (receiverStarted && !ad_end()) {
    LOG_ERR("AIRDROP", "Listener did not stop; restarting");
    ad_logf("listener did not stop");
    saveLog();
    silentRestart();
  }
  receiverStarted = false;
  if (radioStarted && awdl_end() != ESP_OK) {
    LOG_ERR("AIRDROP", "Radio did not stop; restarting");
    ad_logf("radio did not stop");
    saveLog();
    silentRestart();
  }
  radioStarted = false;
  abortFile();
  ad_logf("stopped");
  saveLog();
}

void AirDropActivity::onExit() {
  stop();
  powerManager.setPowerSaving(true);
  Activity::onExit();
}

void AirDropActivity::leave(const bool showFiles) {
  leaving = true;
  stop();
  // Like Lion's other radio screens: a restart hands Home (or Files) a clean heap and a full start.
  if (showFiles) {
    silentRestartToAirDropFiles();
  } else {
    silentRestart();
  }
  activityManager.goHome();  // reached only when the restart turned into sleep
}

void AirDropActivity::setState(const State next) {
  if (state == next) return;
  ad_logf("screen %d -> %d", static_cast<int>(state), static_cast<int>(next));
  state = next;
  startedAt = millis();  // the 10 minutes count from the last thing that happened
  requestUpdate();
}

void AirDropActivity::answerRequest(const bool accept) {
  if (state != State::Asking || !ad_request_pending() || !ad_request_answer(accept)) return;
  ad_logf("user %s", accept ? "accept" : "decline");
  receivedBytes = drawnBytes = 0;
  finished = false;
  lastProgressDraw = millis();
  setState(accept ? State::Receiving : State::Declined);
}

uint8_t AirDropActivity::actions(const char** labels, Action* acts) const {
  uint8_t n = 0;
  auto add = [&](const char* label, const Action act) {
    labels[n] = label;
    acts[n++] = act;
  };
  switch (state) {
    case State::Waiting:
      add(tr(STR_AIRDROP_STOP), Action::Stop);
      break;
    case State::Asking:
      add(tr(STR_AIRDROP_ACCEPT), Action::Accept);
      add(tr(STR_AIRDROP_DECLINE), Action::Decline);
      break;
    case State::Receiving:
      add(tr(STR_CANCEL), Action::Stop);
      break;
    case State::Saved: {
      if (filesThisTransfer == 1 && readable(savedPath)) add(tr(STR_READ), Action::Read);
      add(tr(STR_AIRDROP_OPEN), Action::ShowFiles);
      add(tr(STR_DONE), Action::Stop);
      break;
    }
    case State::Declined:
    case State::TooLarge:
    case State::Error:
      add(tr(STR_OK_BUTTON), Action::Ok);
      break;
  }
  return n;
}

void AirDropActivity::run(const Action action) {
  switch (action) {
    case Action::Accept:
    case Action::Decline:
      answerRequest(action == Action::Accept);
      return;
    case Action::Read:
      APP_STATE.openEpubPath = savedPath;
      APP_STATE.saveToFile();
      leaving = true;
      stop();
      silentRestartToReader();
      activityManager.goHome();  // reached only when the restart turned into sleep
      return;
    case Action::ShowFiles:
      leave(true);
      return;
    case Action::Stop:
      leave(false);
      return;
    case Action::Ok:
      if (receiverStarted) {
        setState(State::Waiting);
      } else {
        leave(false);
      }
      return;
  }
}

bool AirDropActivity::receiveStream(void* ctx, const AdStreamEvent event, const uint8_t* data, const uint32_t len) {
  auto* self = static_cast<AirDropActivity*>(ctx);
  switch (event) {
    case AdStreamEvent::Begin:
      self->abortFile();
      self->filesThisTransfer = 0;
      self->finished = false;
      self->photos.clear();
      self->archive.reset({self,
                           [](void* p, const char* name, uint32_t size) {
                             return static_cast<AirDropActivity*>(p)->beginFile(name, size);
                           },
                           [](void* p, const uint8_t* bytes, uint32_t size) {
                             return static_cast<AirDropActivity*>(p)->writeFile(bytes, size);
                           },
                           [](void* p) { return static_cast<AirDropActivity*>(p)->endFile(); }},
                          RECEIVE_LIMIT);
      return true;
    case AdStreamEvent::Data: {
      const bool ok = self->archive.feed(data, len);
      if (!ok && self->archive.tooLarge()) ad_reject_too_large();
      return ok;
    }
    case AdStreamEvent::Finish:
      if (!self->archive.complete()) {
        ad_logf("archive incomplete");
        return false;
      }
      ad_logf("saved files=%lu last=%s", static_cast<unsigned long>(self->filesThisTransfer), self->savedPath);
      self->finished = true;
      return true;
    case AdStreamEvent::Abort:
      ad_logf("abort written=%lu of %lu", static_cast<unsigned long>(self->writtenSize),
              static_cast<unsigned long>(self->expectedSize));
      self->abortFile();
      if (self->state != State::TooLarge) self->setState(State::Error);
      return true;
  }
  return false;
}

void AirDropActivity::abortFile() {
  if (output) output.close();
  if (temporaryOwned) Storage.remove(temporaryPath);
  temporaryOwned = false;
  temporaryPath[0] = 0;
}

bool AirDropActivity::beginFile(const char* name, const uint32_t len) {
  // The name comes from the phone: one plain path component only.
  if (!name || !*name || !strcmp(name, ".") || !strcmp(name, "..")) return false;
  ad_logf("file %s size=%lu", name, static_cast<unsigned long>(len));
  if (++filesThisTransfer > MAX_FILES) return false;  // one Accept must not flood the card
  for (const unsigned char* p = reinterpret_cast<const unsigned char*>(name); *p; ++p)
    if (*p < 32 || *p == '/' || *p == '\\' || *p == ':') {
      ad_logf("refused name");
      return false;
    }
  if (!Storage.ensureDirectoryExists(RECEIVE_DIR)) return false;
  if (snprintf(savedPath, sizeof(savedPath), "%s/%s", RECEIVE_DIR, name) >= static_cast<int>(sizeof(savedPath)))
    return false;
  unsigned collision = 0;
  while (Storage.exists(savedPath)) {
    if (++collision > 9999) return false;
    snprintf(temporaryPath, sizeof(temporaryPath), "%s/%u", RECEIVE_DIR, collision);
    if (Storage.exists(temporaryPath)) continue;
    if (!Storage.mkdir(temporaryPath)) return false;
    snprintf(savedPath, sizeof(savedPath), "%s/%u/%s", RECEIVE_DIR, collision, name);
  }
  // A short staging name keeps the filesystem's full basename limit for the final name.
  unsigned staging = 0;
  do {
    if (++staging > 9999) return false;
    snprintf(temporaryPath, sizeof(temporaryPath), "%s/.incoming-%u.part", RECEIVE_DIR, staging);
  } while (Storage.exists(temporaryPath));
  temporaryOwned = true;
  if (!Storage.openFileForWrite("AIRDROP", temporaryPath, output)) return false;
  expectedSize = len;
  writtenSize = 0;
  return true;
}

bool AirDropActivity::writeFile(const uint8_t* data, const uint32_t len) {
  if (!output || len > expectedSize - writtenSize) return false;
  for (uint32_t offset = 0; offset < len;) {
    const size_t chunk = std::min<uint32_t>(4096, len - offset);
    if (output.write(data + offset, chunk) != chunk) return false;
    offset += chunk;
    writtenSize += chunk;
    receivedBytes = receivedBytes + chunk;
    delay(1);  // lets the radio tasks run between SD writes
  }
  return true;
}

bool AirDropActivity::endFile() {
  if (!output) return false;
  output.flush();
  bool ok = writtenSize == expectedSize && output.size() == expectedSize;
  ok = output.close() && ok;
  if (ok) ok = Storage.rename(temporaryPath, savedPath);
  if (!ok) {
    ad_logf("file not saved written=%lu expected=%lu", static_cast<unsigned long>(writtenSize),
            static_cast<unsigned long>(expectedSize));
    abortFile();
    return false;
  }
  if (FsHelpers::hasJpgExtension(std::string_view(savedPath)) && photos.size() < MAX_PHOTOS) photos.emplace_back(savedPath);
  temporaryPath[0] = 0;
  temporaryOwned = false;
  return true;
}

// iPhones send photos as JPEG (we advertise no HEIF support); the reader shows BMP, so each
// photo also gets a screen-sized BMP next to it, ready for the file list and sleep screens.
void AirDropActivity::convertPhotos() {
  for (const std::string& jpg : photos) {
    const std::string bmp = jpg.substr(0, jpg.rfind('.')) + ".bmp";
    if (Storage.exists(bmp.c_str())) continue;
    HalFile in, out;
    bool ok = Storage.openFileForRead("AIRDROP", jpg, in) && Storage.openFileForWrite("AIRDROP", bmp, out) &&
              JpegToBmpConverter::jpegFileToBmpStream(in, out);
    if (in) in.close();
    if (out) ok = out.close() && ok;
    if (!ok) Storage.remove(bmp.c_str());
    ad_logf("photo bmp ok=%d %s", ok, bmp.c_str());
    if (ok && filesThisTransfer == 1) snprintf(savedPath, sizeof(savedPath), "%s", bmp.c_str());
  }
  photos.clear();
}

void AirDropActivity::loop() {
  if (leaving) return;
  if (!drawn) {
    drawn = true;
    requestUpdate();
  }
  if (receiverStarted) {
    ad_poll();
    if (leaving) return;
    if (ad_request_pending()) {
      if (state != State::Asking) {
        AdAskInfo request;
        const bool known = ad_request_info(&request);
        RenderLock lock(*this);
        snprintf(askName, sizeof(askName), "%s", known ? request.filename : "");
        askFiles = known ? request.fileCount : 0;
        askBytes =
            known && request.sizesKnown ? static_cast<uint32_t>(std::min<uint64_t>(request.totalBytes, UINT32_MAX)) : 0;
      }
      setState(State::Asking);
    } else if (state == State::Asking) {
      setState(State::Waiting);  // the iPhone cancelled
    }
    if (finished && !ad_upload_pending()) {
      finished = false;
      convertPhotos();
      setState(State::Saved);
    }
    if (millis() - lastStats >= 1000) {
      lastStats = millis();
      saveLog();
      ad_stats_read(&stats);
      if (stats.file_err != observedErrors) {
        observedErrors = stats.file_err;
        if (state != State::TooLarge) setState(State::Error);
      }
      if (stats.limit_rejections != observedLimitRejections) {
        observedLimitRejections = stats.limit_rejections;
        setState(State::TooLarge);
      }
    }
    // E-ink: progress at most every few seconds, and only when it moved.
    if (state == State::Receiving && receivedBytes != drawnBytes && millis() - lastProgressDraw >= PROGRESS_DRAW_MS) {
      lastProgressDraw = millis();
      drawnBytes = receivedBytes;
      requestUpdate();
    }
  }
  if (state == State::Declined && millis() - startedAt >= DECLINED_SHOW_MS) setState(State::Waiting);
  if (state != State::Asking && state != State::Receiving && millis() - startedAt >= SESSION_MS &&
      !ad_upload_pending()) {
    leave(false);
    return;
  }

  const char* labels[3];
  Action acts[3];
  const uint8_t count = actions(labels, acts);
  int tx = 0;
  int ty = 0;
  if (mappedInput.hasTouch() && mappedInput.wasScreenTouchDown(tx, ty)) {
    const int index = TouchActionButtons::indexAt(actionLayout(renderer, count), tx, ty);
    if (index >= 0) {
      run(acts[index]);
      return;
    }
  }
  if (TouchHeaderBackButton::wasTapped(mappedInput, renderer) || mappedInput.wasHomeGesture()) {
    run(state == State::Asking ? Action::Decline : Action::Stop);
  } else if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    run(acts[count - 1]);
  } else if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    run(acts[0]);
  } else if (count == 3 && mappedInput.wasReleased(MappedInputManager::Button::Left)) {
    run(acts[1]);
  }
  delay(5);
}

void AirDropActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const char* title = tr(STR_AIRDROP);
  if (mappedInput.hasTouchHardware()) {
    TouchHeaderBackButton::drawCompact(renderer, title);
  } else {
    CompactHeader::drawTitle(renderer, title);
  }

  const auto& metrics = UITheme::getInstance().getMetrics();
  const int width = renderer.getScreenWidth();
  const Rect area{metrics.contentSidePadding, 0, width - metrics.contentSidePadding * 2, renderer.getScreenHeight()};
  const int line = renderer.getLineHeight(UI_10_FONT_ID);
  int y = renderer.getScreenHeight() / 2 - line * 3;
  auto heading = [&](const char* text) {
    y += UITheme::drawCenteredWrappedText(renderer, area, UI_10_FONT_ID, y, text, 3, true, EpdFontFamily::BOLD) +
         metrics.verticalSpacing;
  };
  auto body = [&](const char* text, const int lines = 3) {
    if (!text || !*text) return;
    y += UITheme::drawCenteredWrappedText(renderer, area, UI_10_FONT_ID, y, text, lines) + metrics.verticalSpacing;
  };

  char text[96];
  char size[24];
  char total[24];
  if (!drawn) {
    heading(tr(STR_AIRDROP_STARTING));
  } else {
    switch (state) {
      case State::Waiting:
        heading(tr(STR_AIRDROP_WAITING));
        snprintf(text, sizeof(text), tr(STR_AIRDROP_WAITING_DETAIL), ad_receiver_name());
        body(text);
        break;
      case State::Asking:
        if (askFiles > 1) {
          snprintf(text, sizeof(text), tr(STR_AIRDROP_ASK_FILES), static_cast<unsigned>(askFiles));
          heading(text);
        } else {
          heading(tr(STR_AIRDROP_ASK_FILE));
        }
        body(askName);
        if (askBytes) {
          formatMb(size, sizeof(size), askBytes);
          body(size, 1);
        }
        break;
      case State::Receiving: {
        const uint32_t done = receivedBytes;
        heading(done || stats.upload_pending ? tr(STR_AIRDROP_RECEIVING) : tr(STR_AIRDROP_WAIT_UPLOAD));
        body(askName, 2);
        formatMb(size, sizeof(size), done);
        if (askBytes) {
          y += metrics.verticalSpacing;
          GUI.drawProgressBar(renderer, Rect{area.x, y, area.width, metrics.progressBarHeight},
                              std::min(done, askBytes) / 1024, std::max<uint32_t>(1, askBytes / 1024));
          y += metrics.progressBarHeight + line + metrics.verticalSpacing * 2;  // below the bar's own "40%"
          formatMb(total, sizeof(total), askBytes);
          snprintf(text, sizeof(text), tr(STR_AIRDROP_PROGRESS), size, total);
          body(text, 1);
        } else {
          body(size, 1);
        }
        break;
      }
      case State::Saved:
        heading(tr(STR_AIRDROP_SAVED));
        body(filesThisTransfer > 1 ? RECEIVE_DIR : savedPath);
        if (filesThisTransfer == 1 && !listed(savedPath)) body(tr(STR_AIRDROP_CANT_OPEN));
        break;
      case State::Declined:
        heading(tr(STR_AIRDROP_DECLINED));
        body(tr(STR_AIRDROP_DECLINED_DETAIL));
        break;
      case State::TooLarge:
        heading(tr(STR_AIRDROP_TOO_LARGE));
        break;
      case State::Error:
        heading(tr(STR_AIRDROP_FAILED));
        break;
    }
  }

  const char* labels[3];
  Action acts[3];
  const uint8_t count = actions(labels, acts);
  if (mappedInput.hasTouch()) {
    TouchActionButtons::draw(renderer, actionLayout(renderer, count), labels, 0, -1, UI_10_FONT_ID);
  } else {
    const auto hints = mappedInput.mapLabels(mappedInput.withBackArrow(labels[count - 1]), count > 1 ? labels[0] : "",
                                             count == 3 ? labels[1] : "", "");
    GUI.drawButtonHints(renderer, hints.btn1, hints.btn2, hints.btn3, hints.btn4);
  }
  renderer.displayBuffer();
}
#endif
