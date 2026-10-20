#include "FrontlightPanelActivity.h"

#include <SpeedLog.h>

#include <CrossInkHalFrontlight.h>
#include <GfxRenderer.h>
#include <I18n.h>
#include <Memory.h>

#include <WiFi.h>

#include <algorithm>
#include <cstdio>

#include "CrossPointSettings.h"
#include "GlobalActions.h"
#include "MappedInputManager.h"
#include "activities/ActivityManager.h"
#include "activities/RenderLock.h"
#include "activities/home/BookActions.h"
#include "activities/network/WifiSelectionActivity.h"
#include "activities/settings/SettingsActivity.h"
#include "components/DrawerHandle.h"
#include "components/HeaderDate.h"
#include "components/TouchHeaderBackButton.h"
#include "components/UITheme.h"
#include "components/UIThemeTokens.h"
#include "components/UiAppHelpers.h"
#include "components/icons/frontlightHeaderIcons.h"
#include "components/icons/listIcons.h"
#include "components/icons/readingStatsIcons.h"
#include "components/icons/tablerIcons.h"
#include "components/icons/touchscreenStateIcons.h"
#include "components/icons/varaghIcons.h"
#include "network/OnlineWifi.h"
#include "network/WifiUtils.h"
#include "util/FrontlightNight.h"
#include "util/ReaderBookProgress.h"

namespace fui = freeink::ui;

namespace {
constexpr fui::ActionId ACTION_BRIGHTNESS = 1;
constexpr fui::ActionId ACTION_WARMTH = 2;
constexpr fui::ActionId ACTION_TOGGLE = 3;
constexpr fui::ActionId ACTION_BRIGHTNESS_STEP = 4;
constexpr fui::ActionId ACTION_WARMTH_STEP = 5;
constexpr fui::ActionId ACTION_QUICK = 6;
constexpr fui::ActionId ACTION_DISMISS = 7;
constexpr fui::ActionId ACTION_CC_BUTTON = 8;

// Control Center layout (design C). The card starts below the battery row,
// which ends at y 26 in every theme.
constexpr int CC_CARD_TOP = 34;
constexpr int CC_CARD_SIDE = 12;
constexpr int CC_CARD_PAD = 22;
constexpr int CC_CARD_RADIUS = 28;
constexpr int CC_LIGHT_H = 84;
constexpr int CC_KNOB_W = 72;  // round knob; "100%" in UI_10 bold is 58 px
constexpr int CC_STEP_D = 52;   // round - / + buttons beside the light slider
constexpr int CC_STEP_GAP = 8;
constexpr int CC_SMALL_STEP_D = 40;  // - / + beside the warmth slider
constexpr int CC_TRACK_H = 12;  // thin track under both knobs
constexpr int CC_WARM_KNOB_D = 34;
constexpr int CC_WARMTH_GAP = 18;
constexpr int CC_WARMTH_H = 40;
constexpr int CC_ICON = 24;
constexpr int CC_BUTTONS_GAP = 26;
constexpr int CC_PILL_W = 92;
constexpr int CC_PILL_H = 60;
constexpr int CC_ROW_PITCH = 104;  // pill + label + gap
constexpr int CC_GRABBER_BAND = 24;

const freeink::Icon* controlButtonIcon(const uint8_t button, const bool on) {
  switch (button) {
    case CrossPointSettings::CC_TAP_TO_LOOK_UP: return &icon_v_dictionary_24;
    case CrossPointSettings::CC_WIFI: return &icon_v_wifi_24;
    case CrossPointSettings::CC_DARK: return &icon_v_contrast_24;
    case CrossPointSettings::CC_TOUCH_LOCK: return &icon_v_lock_24;
    case CrossPointSettings::CC_LIGHT: return on ? &icon_v_bulb_on_24 : &icon_v_bulb_24;
    case CrossPointSettings::CC_NIGHT_LIGHT: return &icon_v_moon_24;
    case CrossPointSettings::CC_GUIDE_DOTS: return &icon_v_more_24;
    case CrossPointSettings::CC_FOCUS_READING: return &icon_v_text_size_24;
    case CrossPointSettings::CC_REFRESH: return &icon_v_refresh_24;
    case CrossPointSettings::CC_SYNC: return &icon_v_sync_24;
    case CrossPointSettings::CC_SLEEP: return &icon_v_power_24;
    case CrossPointSettings::CC_SETTINGS: return &icon_v_gear_24;
    case CrossPointSettings::CC_READING_STATS: return &icon_v_stats_24;
    case CrossPointSettings::CC_LIBRARY: return &icon_v_library_24;
    case CrossPointSettings::CC_APPS: return &icon_v_apps_24;
    case CrossPointSettings::CC_FILE_TRANSFER: return &icon_v_transfer_24;
    case CrossPointSettings::CC_ROTATE: return &icon_v_rotate_24;
    default: return nullptr;
  }
}

StrId controlButtonLabel(const uint8_t button) {
  static constexpr StrId LABELS[] = {
      StrId::STR_CC_EMPTY,       StrId::STR_CC_TAP_TO_LOOK_UP, StrId::STR_CC_WIFI,
      StrId::STR_CC_DARK,        StrId::STR_CC_TOUCH_LOCK,     StrId::STR_CC_LIGHT,
      StrId::STR_CC_NIGHT_LIGHT, StrId::STR_CC_GUIDE_DOTS,     StrId::STR_CC_FOCUS_READING,
      StrId::STR_CC_REFRESH,     StrId::STR_CC_SYNC,           StrId::STR_CC_SLEEP,
      StrId::STR_CC_SETTINGS,    StrId::STR_CC_READING_STATS,  StrId::STR_CC_LIBRARY,
      StrId::STR_CC_APPS,        StrId::STR_CC_FILE_TRANSFER,  StrId::STR_CC_ROTATE};
  return button < CrossPointSettings::CONTROL_CENTER_BUTTON_COUNT ? LABELS[button] : StrId::STR_CC_EMPTY;
}
constexpr int HEADER_ICON_SIZE = 24;
constexpr int HEADER_BUTTON_WIDTH = 64;
constexpr int HEADER_CONTENT_BOTTOM_GAP = 8;
constexpr int ACTION_BAR_HEIGHT = 58;

uint8_t percentFromPermille(const int16_t permille) {
  int value = (static_cast<int>(permille) * 100 + 500) / 1000;
  if (value < 0) value = 0;
  if (value > 100) value = 100;
  return static_cast<uint8_t>(value);
}

fui::SheetProps frontlightSheetProps() {
  fui::SheetProps sheet;
  sheet.anchor = fui::SheetEdge::Top;
  sheet.dismissAction = ACTION_DISMISS;
  sheet.radius = 0;
  return sheet;
}
}  // namespace

FrontlightPanelActivity::FrontlightPanelActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                                 FrontlightPanelContext context)
    : Activity("FrontlightPanel", renderer, mappedInput),
      context(std::move(context)),
      drawerState(this->context.drawerState),
      uiTarget(makeUiTarget(renderer)),
      app(uiTarget, uiTarget.deviceContext()) {}

void FrontlightPanelActivity::onEnter() {
  Activity::onEnter();

  // The HAL is the live source of truth; SETTINGS only mirrors it for boot.
  brightness = Frontlight.brightness();
  warmth = Frontlight.warmth();
  lightOn = Frontlight.isOn();
  initialInversion = SETTINGS.screenInverted;
  initialTouchscreenDisabled = SETTINGS.disableReaderTouchscreen;
  pendingTouchscreenDisabled = initialTouchscreenDisabled;
  mappedInput.setReaderTouchscreenOverride(true);

  uiReady = false;
  applySharedUiTheme(app, uiTarget);
  app.on(ACTION_BRIGHTNESS, &FrontlightPanelActivity::onBrightnessEvent, this);
  app.on(ACTION_WARMTH, &FrontlightPanelActivity::onWarmthEvent, this);
  app.on(ACTION_TOGGLE, &FrontlightPanelActivity::onToggleEvent, this);
  app.on(ACTION_BRIGHTNESS_STEP, &FrontlightPanelActivity::onBrightnessStepEvent, this);
  app.on(ACTION_WARMTH_STEP, &FrontlightPanelActivity::onWarmthStepEvent, this);
  app.on(ACTION_QUICK, &FrontlightPanelActivity::onQuickActionEvent, this);
  app.on(ACTION_DISMISS, &FrontlightPanelActivity::onDismissEvent, this);
  app.on(ACTION_CC_BUTTON, &FrontlightPanelActivity::onControlButtonEvent, this);
  app.setScreen(&FrontlightPanelActivity::panelScreen, this);
  prepareReaderDetailsLayout();
  requestUpdate();
}

void FrontlightPanelActivity::onExit() {
  // Debounced persistence: one SPIFFS write on close, never per slider tick.
  const bool inversionChanged = initialInversion != static_cast<bool>(SETTINGS.screenInverted);
  const bool touchscreenChanged = initialTouchscreenDisabled != pendingTouchscreenDisabled;
  SETTINGS.disableReaderTouchscreen = pendingTouchscreenDisabled ? 1 : 0;
  const bool frontlightChanged =
      !context.showReaderDetails && (SETTINGS.frontlightBrightness != brightness ||
                                     SETTINGS.frontlightWarmth != warmth || SETTINGS.frontlightOn != (lightOn ? 1 : 0));
  if (frontlightChanged || inversionChanged || touchscreenChanged || settingsDirty) {
    if (!context.showReaderDetails) {
      SETTINGS.frontlightBrightness = brightness;
      SETTINGS.frontlightWarmth = warmth;
      SETTINGS.frontlightOn = lightOn ? 1 : 0;
    }
    // The drawer can be opened over a dictionary or another reader child.
    // Find the owning reader so its per-book values stay out of the global file.
    activityManager.persistGlobalSettings();
  }
  mappedInput.setReaderTouchscreenOverride(false);
  Activity::onExit();
}

void FrontlightPanelActivity::onExternalFrontlightChange() {
  brightness = Frontlight.brightness();
  warmth = Frontlight.warmth();
  lightOn = Frontlight.isOn();
  requestUpdate();
}

void FrontlightPanelActivity::onBrightnessEvent(const fui::ActionEvent& event, void* user) {
  auto* self = static_cast<FrontlightPanelActivity*>(user);
  if (event.dragPermille < 0) return;
  const uint8_t next =
      self->context.showReaderDetails
          ? percentFromPermille(event.dragPermille)
          : lightPercentFromPermille(travelPermille(event.dragPermille, self->lightSliderW, CC_KNOB_W));
  if (next != self->brightness) {
    self->brightness = next;
    Frontlight.setBrightness(next);
    self->sliderMoved = true;
  }
  // Adjusting brightness while off is an obvious "I want light" intent.
  if (!self->lightOn) {
    self->lightOn = true;
    Frontlight.setOn(true);
    self->sliderMoved = true;
  }
}

void FrontlightPanelActivity::onWarmthEvent(const fui::ActionEvent& event, void* user) {
  auto* self = static_cast<FrontlightPanelActivity*>(user);
  if (event.dragPermille < 0) return;
  const uint8_t next = percentFromPermille(self->context.showReaderDetails
                                               ? event.dragPermille
                                               : travelPermille(event.dragPermille, self->warmthSliderW, CC_WARM_KNOB_D));
  if (next == self->warmth) return;
  self->warmth = next;
  Frontlight.setWarmth(next);
  self->sliderMoved = true;
}

void FrontlightPanelActivity::onBrightnessStepEvent(const fui::ActionEvent& event, void* user) {
  // Always 1 %: the steps are for exact adjustment. The new number is the
  // feedback; a grey tap flash would cost two more screen refreshes per tap.
  auto* self = static_cast<FrontlightPanelActivity*>(user);
  self->app.clearTapFlash();
  self->adjustBrightness(event.value);
}

void FrontlightPanelActivity::onWarmthStepEvent(const fui::ActionEvent& event, void* user) {
  auto* self = static_cast<FrontlightPanelActivity*>(user);
  self->app.clearTapFlash();
  self->adjustWarmth(event.value);
}

void FrontlightPanelActivity::onControlButtonEvent(const fui::ActionEvent& event, void* user) {
  auto* self = static_cast<FrontlightPanelActivity*>(user);
  self->activateControlButton(SETTINGS.controlCenterSlot(event.value), event.longPress);
}

void FrontlightPanelActivity::onToggleEvent(const fui::ActionEvent&, void* user) {
  static_cast<FrontlightPanelActivity*>(user)->toggleLight();
}

void FrontlightPanelActivity::onQuickActionEvent(const fui::ActionEvent& event, void* user) {
  static_cast<FrontlightPanelActivity*>(user)->activateQuickAction(event.value);
}

void FrontlightPanelActivity::onDismissEvent(const fui::ActionEvent&, void* user) {
  static_cast<FrontlightPanelActivity*>(user)->close();
}

void FrontlightPanelActivity::adjustBrightness(const int delta) {
  int next = static_cast<int>(brightness) + delta;
  if (next < 0) next = 0;
  if (next > 100) next = 100;
  if (next == brightness) return;
  brightness = static_cast<uint8_t>(next);
  Frontlight.setBrightness(brightness);
  if (!lightOn) {
    lightOn = true;
    Frontlight.setOn(true);
  }
  requestUpdate();
}

void FrontlightPanelActivity::adjustWarmth(const int delta) {
  int next = static_cast<int>(warmth) + delta;
  if (next < 0) next = 0;
  if (next > 100) next = 100;
  if (next == warmth) return;
  warmth = static_cast<uint8_t>(next);
  Frontlight.setWarmth(warmth);
  requestUpdate();
}

void FrontlightPanelActivity::toggleLight() {
  lightOn = !lightOn;
  Frontlight.setOn(lightOn);
  requestUpdate();
}

void FrontlightPanelActivity::toggleReaderTouchscreen() {
  pendingTouchscreenDisabled = !pendingTouchscreenDisabled;
  {
    RenderLock lock;
    BookActions::drawToast(renderer,
                           pendingTouchscreenDisabled ? tr(STR_TOUCHSCREEN_DISABLED) : tr(STR_TOUCHSCREEN_ENABLED));
  }
  delay(1000);
  requestUpdate();
}

void FrontlightPanelActivity::close() {
  FrontlightPanelResult result;
  result.state = drawerState;
  result.activeEpub = context.activeEpub;
  result.ttfRenderingChanged = ttfRenderingChanged;
  result.bookPath = context.bookPath;
  setResult(ActivityResult(std::move(result)));
  finish();
}

void FrontlightPanelActivity::openReadingStats() {
  if (!context.showReadingStatsAction || !SETTINGS.shouldTrackReadingStats()) return;
  if (!context.readingStatsActivity && context.sourceActivity) {
    context.readingStatsActivity = context.sourceActivity->createFrontlightReadingStatsActivity();
  }
  if (!context.readingStatsActivity) return;
  startActivityForResult(std::move(context.readingStatsActivity), [this](const ActivityResult&) { close(); });
}

void FrontlightPanelActivity::openGlobalSettings() {
  // A drawer over Settings must not end the outer screen's edit session.
  const bool startedGlobalEdit = activityManager.beginGlobalSettingsEdit();
  auto settings = makeUniqueNoThrow<SettingsActivity>(renderer, mappedInput, true, true);
  if (!settings) {
    LOG_ERR("LIGHT", "OOM opening Settings from frontlight panel");
    if (startedGlobalEdit) activityManager.endGlobalSettingsEdit();
    return;
  }
  startActivityForResult(std::move(settings), [this, startedGlobalEdit](const ActivityResult& result) {
    if (const auto* options = std::get_if<TtfRenderOptionsResult>(&result.data)) {
      ttfRenderingChanged = ttfRenderingChanged || options->activeFamilyChanged;
    }
    if (startedGlobalEdit) activityManager.endGlobalSettingsEdit();
    close();
  });
}

void FrontlightPanelActivity::openSyncDialog() {
  static constexpr std::array<StrId, 3> OPTIONS = {StrId::STR_SYNC_PROGRESS, StrId::STR_NEARBY_POSITION_SYNC,
                                                   StrId::STR_SEND_NEARBY_BOOK};
  drawerState.syncDialogOpen = true;
  const bool canSyncBookProgress = context.activeEpub;
  const bool canSendBook = !context.bookPath.empty();
  optionPopup.show(StrId::STR_SYNC_AND_TRANSFER, OPTIONS.data(), OPTIONS.size(), canSyncBookProgress ? 0 : 2,
                   [this](const int index) {
                     drawerState.syncDialogOpen = false;
                     FrontlightPanelResult panelResult;
                     panelResult.state = drawerState;
                     panelResult.activeEpub = context.activeEpub;
                     panelResult.bookPath = context.bookPath;
                     if (index == 0) panelResult.action = FrontlightPanelAction::SyncProgress;
                     if (index == 1) panelResult.action = FrontlightPanelAction::NearbyPositionSync;
                     if (index == 2) panelResult.action = FrontlightPanelAction::SendNearbyBook;
                     setResult(ActivityResult(std::move(panelResult)));
                     finish();
                   });
  optionPopup.setDisabledOptions({!canSyncBookProgress, !canSyncBookProgress, !canSendBook});
  optionPopup.setCancelCallback([this] { closeSyncDialog(); });
  requestUpdate();
}

void FrontlightPanelActivity::closeSyncDialog() {
  drawerState.syncDialogOpen = false;
  close();
}

void FrontlightPanelActivity::activateQuickAction(const int index) {
  if (index < 0 || index >= 5) return;
  if (index == 0 && !context.showReadingStatsAction) return;
  drawerState.selectedAction = static_cast<int8_t>(index);
  switch (index) {
    case 0:
      openReadingStats();
      return;
    case 1:
      openSyncDialog();
      return;
    case 2:
      SETTINGS.screenInverted = SETTINGS.screenInverted ? 0 : 1;
      display.setInverted(SETTINGS.screenInverted != 0);
      requestUpdate();
      return;
    case 3:
      openGlobalSettings();
      return;
    case 4:
      toggleReaderTouchscreen();
      return;
  }
}

bool FrontlightPanelActivity::handleHomeGesture() {
  if (context.activeReaderBook) {
    activityManager.goHome();
    return true;
  }
  close();
  return true;
}

void FrontlightPanelActivity::loop() {
  if (optionPopup.handleInput(mappedInput, [this] { requestUpdate(); })) return;

  // A power-button shortcut can change the light globally while this panel is
  // open. Keep the panel's state and eventual persistence in sync.
  if (!context.showReaderDetails && lightOn != Frontlight.isOn()) {
    lightOn = Frontlight.isOn();
    requestUpdate();
  }

  const Rect homeButton = homeButtonRect();
  if (context.activeReaderBook &&
      mappedInput.wasTapInRect(homeButton.x, homeButton.y, homeButton.width, homeButton.height)) {
    activityManager.goHome();
    return;
  }

  if (DrawerHandle::wasDismissSwipe(mappedInput, drawerHandleRect, fui::SheetEdge::Top)) {
    close();
    return;
  }

  fui::InputSnapshot snap{};
  if (uiReady) {
    snap = touchSnapshotFrom(mappedInput);
    if (snap.touchPressed || snap.touchHeld || snap.touchReleased) {
      sliderMoved = false;
      const auto event = app.route(snap);
      // Each frame is a ~250 ms panel refresh: a finger jitter that left the
      // value as it was must not queue one.
      const bool idleDrag = event && event.dragPermille >= 0 && !sliderMoved;
      if (app.invalidated() && !idleDrag) {
        if (event) lastInputMs = millis();
        requestUpdate();
      }
      if (event) {
        if (event.dragPermille >= 0) draggingSlider = true;
        return;
      }
    }
    if (draggingSlider) {
      // Drag ended (possibly off the slider): swallow the release's swipe so
      // it can't double as the left-edge back gesture and close the panel.
      if (!snap.touchHeld) draggingSlider = false;
      return;
    }
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    close();
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    if (!context.showReaderDetails) toggleLight();
    return;
  }

  if (context.showReaderDetails) return;

  buttonNavigator.onPressAndContinuous({MappedInputManager::Button::Left},
                                       [this] { adjustBrightness(-1); });
  buttonNavigator.onPressAndContinuous({MappedInputManager::Button::Right},
                                       [this] { adjustBrightness(1); });
}

Rect FrontlightPanelActivity::homeButtonRect() const {
  const auto& metrics = UITheme::getInstance().getMetrics();
  return Rect{renderer.getScreenWidth() - HEADER_BUTTON_WIDTH, metrics.topPadding, HEADER_BUTTON_WIDTH,
              TouchHeaderBackButton::height(metrics, mappedInput)};
}

int FrontlightPanelActivity::computePanelBottom() const {
  // Mirror buildPanelScreen's takeTop/spacer sequence so the frame, content
  // margin, and dismiss threshold land on the same edge.
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto tokens = uiThemeTokens(uiTarget);
  const int16_t lh = uiTarget.lineHeight(tokens.bodyText.font);
  int y = metrics.topPadding + TouchHeaderBackButton::height(metrics, mappedInput);
  if (showsBookProgress()) y += metrics.tabBarHeight;
  if (context.showReaderDetails) {
    const int16_t titleLh = uiTarget.lineHeight(tokens.titleText.font);
    y += tokens.spaceLg * 2;
    y += titleLh * static_cast<int>(readerTitleLines.size());
    if (!readerTitleLines.empty()) y += tokens.spaceLg;
    if (!context.bookDetails.chapter.empty() || !context.bookDetails.author.empty()) {
      y += lh + tokens.spaceLg;
    }
    y += lh;
    y += tokens.spaceLg * 3 + ACTION_BAR_HEIGHT;
    const auto sheet = frontlightSheetProps();
    return y + DrawerHandle::bandHeight(sheet);
  }
  return controlCenterBottom();
}

void FrontlightPanelActivity::prepareReaderDetailsLayout() {
  readerTitleLines.clear();
  if (!context.showReaderDetails || context.bookDetails.title.empty()) return;
  const auto tokens = uiThemeTokens(uiTarget);
  const int16_t sideInset = static_cast<int16_t>(tokens.spaceLg * 2);
  const int width = std::max<int>(1, renderer.getScreenWidth() - sideInset * 2);
  readerTitleLines =
      renderer.wrappedText(UI_12_FONT_ID, context.bookDetails.title.c_str(), width, 2, EpdFontFamily::BOLD);
}

void FrontlightPanelActivity::drawReaderDetails(UiApp::ScreenType& screen) {
  const auto& theme = screen.theme();
  const fui::Insets sideInset{0, static_cast<int16_t>(theme.spaceLg * 2), 0, static_cast<int16_t>(theme.spaceLg * 2)};
  screen.spacer(static_cast<int16_t>(theme.spaceLg * 2));

  fui::TextStyle title = theme.titleText;
  title.bold = true;
  title.align = fui::TextAlign::Center;
  const int16_t titleLh = screen.target().lineHeight(title.font);
  for (const std::string& line : readerTitleLines) {
    screen.target().text(screen.takeTop(titleLh).inset(sideInset), line.c_str(), title);
  }
  if (!readerTitleLines.empty()) screen.spacer(theme.spaceLg);

  const char* subtitle =
      !context.bookDetails.chapter.empty() ? context.bookDetails.chapter.c_str() : context.bookDetails.author.c_str();
  fui::TextStyle body = theme.bodyText;
  body.align = fui::TextAlign::Center;
  const int16_t bodyLh = screen.target().lineHeight(body.font);
  if (subtitle[0] != '\0') {
    const std::string visible =
        renderer.truncatedText(UI_10_FONT_ID, subtitle, screen.body().width - sideInset.left - sideInset.right);
    screen.target().text(screen.takeTop(bodyLh, theme.spaceLg).inset(sideInset), visible.c_str(), body);
  }

  char progress[48];
  std::snprintf(progress, sizeof(progress), "%d%% %s", context.bookDetails.progressPercent, tr(STR_COMPLETE));
  screen.target().text(screen.takeTop(bodyLh).inset(sideInset), progress, body);
  screen.spacer(static_cast<int16_t>(theme.spaceLg * 3));
}

void FrontlightPanelActivity::panelScreen(UiApp::ScreenType& screen, void* user) {
  static_cast<FrontlightPanelActivity*>(user)->buildPanelScreen(screen);
}

void FrontlightPanelActivity::buildPanelScreen(UiApp::ScreenType& screen) {
  if (!context.showReaderDetails) {
    buildControlCenter(screen);
    return;
  }
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto& theme = screen.theme();
  const fui::SheetProps sheet = frontlightSheetProps();
  const fui::Rect sheetRect{0, 0, static_cast<int16_t>(renderer.getScreenWidth()), static_cast<int16_t>(panelBottom)};
  fui::sheet(screen.frame(), sheetRect, sheet);
  // Body spans from below the header to the sheet's content edge. The grabber
  // owns the remaining band at the bottom of this top-anchored drawer.
  const fui::Rect sheetContent = fui::sheetContentRect(sheetRect, sheet);
  drawerHandleRect = DrawerHandle::registerTap(screen.frame(), sheetContent, sheet, ACTION_DISMISS);
  const int16_t bottomInset = static_cast<int16_t>(renderer.getScreenHeight() - sheetContent.bottom());
  screen.setContentMargin(
      fui::Insets{static_cast<int16_t>(metrics.topPadding + TouchHeaderBackButton::height(metrics, mappedInput) +
                                       (showsBookProgress() ? metrics.tabBarHeight : 0)),
                  0, bottomInset, 0});

  const fui::Rect actionBar = screen.takeBottom(ACTION_BAR_HEIGHT);
  const bool showStats = context.showReadingStatsAction;
  const int16_t slotCount = showStats ? 5 : 4;
  const int16_t slotWidth = static_cast<int16_t>(actionBar.width / slotCount);
  const std::array<fui::BitmapRef, 5> icons = {
      fui::bitmapFromIcon(icon_reading_stats_24), fui::bitmapFromIcon(icon_transfer_24),
      fui::bitmapFromIcon(icon_tabler_moon_filled_24), fui::bitmapFromIcon(icon_sliders_horizontal_24),
      fui::bitmapFromIcon(pendingTouchscreenDisabled ? icon_device_tablet_off_24 : icon_device_tablet_24)};
  for (int16_t slotIndex = 0; slotIndex < slotCount; ++slotIndex) {
    const int16_t i = showStats ? slotIndex : slotIndex + 1;
    const int16_t x = static_cast<int16_t>(actionBar.x + slotIndex * slotWidth);
    const int16_t width = slotIndex == slotCount - 1 ? static_cast<int16_t>(actionBar.right() - x) : slotWidth;
    const fui::Rect slot{x, actionBar.y, width, actionBar.height};
    screen.frame().hit(slot, ACTION_QUICK, i);
    screen.target().bitmap(slot, icons[static_cast<size_t>(i)], fui::BitmapMode::Center);
  }
  screen.target().fill(fui::Rect{actionBar.x, actionBar.y, actionBar.width, 1}, fui::Paint::solid(fui::Color::Black));

  if (context.showReaderDetails) {
    drawReaderDetails(screen);
    return;
  }

  const int16_t lh = screen.target().lineHeight(theme.bodyText.font);
  const int16_t rowH = theme.rowHeight;
  const fui::Insets sideInset{0, static_cast<int16_t>(theme.spaceLg * 2), 0, static_cast<int16_t>(theme.spaceLg * 2)};
  char line[48];

  screen.spacer(theme.spaceLg);

  // Header row: "Brightness NN%" on the left, a tappable bulb icon on the right
  // that toggles the light — `lightbulb` when on, `lightbulb-off` when off (the
  // icon itself is the state indicator). Sharing a row with the label frees the
  // whole bottom toggle row, shrinking the panel.
  const fui::Rect headerRow = screen.takeTop(rowH, theme.spaceSm).inset(sideInset);
  snprintf(line, sizeof(line), "%s  %u%%", tr(STR_BRIGHTNESS), static_cast<unsigned>(brightness));
  const fui::BitmapRef lightIcon = fui::bitmapFromIcon(lightOn ? icon_lightbulb_28 : icon_lightbulb_off_28);
  const int16_t iconW = static_cast<int16_t>(lightIcon.width);
  const int16_t iconH = static_cast<int16_t>(lightIcon.height);
  const fui::Rect iconRect{static_cast<int16_t>(headerRow.x + headerRow.width - iconW),
                           static_cast<int16_t>(headerRow.y + (rowH - iconH) / 2), iconW, iconH};
  const fui::Rect labelRect{headerRow.x, static_cast<int16_t>(headerRow.y + (rowH - lh) / 2),
                            static_cast<int16_t>(headerRow.width - iconW - theme.spaceMd), lh};
  screen.target().text(labelRect, line, theme.bodyText);
  // Keep the visible glyph aligned to the content inset, but let its touch
  // target fill the otherwise blank right edge and the adjacent row gaps.
  // This makes the control more forgiving without reaching the brightness
  // slider below.
  const int16_t hitW = static_cast<int16_t>(iconW + theme.spaceLg * 4);
  const fui::Rect hitRect{static_cast<int16_t>(headerRow.right() - hitW),
                          static_cast<int16_t>(headerRow.y - theme.spaceSm),
                          static_cast<int16_t>(hitW + sideInset.right), static_cast<int16_t>(rowH + theme.spaceSm * 2)};
  screen.frame().hit(hitRect, ACTION_TOGGLE);
  screen.target().bitmap(iconRect, lightIcon, fui::BitmapMode::Center);

  addStepSlider(screen, screen.takeTop(theme.rowHeight, theme.spaceLg).inset(sideInset), brightness, ACTION_BRIGHTNESS,
                ACTION_BRIGHTNESS_STEP);

  if (Frontlight.hasColorTemperature()) {
    snprintf(line, sizeof(line), "%s  %u%%", tr(STR_WARMTH), static_cast<unsigned>(warmth));
    screen.target().text(screen.takeTop(lh, theme.spaceSm).inset(sideInset), line, theme.bodyText);
    addStepSlider(screen, screen.takeTop(theme.rowHeight, theme.spaceLg).inset(sideInset), warmth, ACTION_WARMTH,
                  ACTION_WARMTH_STEP);
  }

  screen.spacer(theme.spaceLg);
}

void FrontlightPanelActivity::drawHeader() {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int headerHeight = TouchHeaderBackButton::height(metrics, mappedInput);
  const Rect header{0, metrics.topPadding, renderer.getScreenWidth(), headerHeight};

  char date[16] = {};
  const char* title = context.showReaderDetails ? "" : tr(STR_FRONTLIGHT);
  int titleFontId = UI_12_FONT_ID;
  if (context.showReaderDetails) {
    if (formatHeaderDateText(date, sizeof(date))) title = date;
    titleFontId = UI_10_FONT_ID;
  } else if (context.activeReaderBook && !context.bookTitle.empty()) {
    title = context.bookTitle.c_str();
    titleFontId = UI_10_FONT_ID;
  } else if (formatHeaderDateText(date, sizeof(date))) {
    title = date;
    titleFontId = UI_10_FONT_ID;
  }

  // Reuse the normal header chrome so the drawer retains its separator, clock,
  // and battery indicator. The date remains deliberately smaller than a page
  // title, matching its secondary status role.
  // The panel is a global overlay even when it was opened from a reader. Its
  // clock and battery therefore follow the outside-reader visibility settings.
  GUI.drawHeader(renderer, header, "", nullptr, false);
  // Keep the title/date in the header's lower lane. The status chrome above
  // owns the clock and battery, so centering across the whole header crowds it.
  const int headerBottom = header.y + header.height;
  const int titleY = headerBottom - HEADER_CONTENT_BOTTOM_GAP - renderer.getLineHeight(titleFontId);
  const bool showBookTitle = !context.showReaderDetails && context.activeReaderBook && !context.bookTitle.empty();
  if (showBookTitle) {
    const auto tokens = uiThemeTokens(uiTarget);
    const Rect homeButton = homeButtonRect();
    const int titleX = header.x + tokens.headerSidePadding;
    const int titleRight = homeButton.x - tokens.spaceSm;
    const int titleWidth = std::max(0, titleRight - titleX);
    const std::string visibleTitle = renderer.truncatedText(titleFontId, title, titleWidth, EpdFontFamily::BOLD);
    renderer.drawText(titleFontId, titleX, titleY, visibleTitle.c_str(), true, EpdFontFamily::BOLD);
  } else {
    UITheme::drawCenteredText(renderer, header, titleFontId, titleY, title, true);
  }

  if (context.activeReaderBook) {
    const Rect button = homeButtonRect();
    uiTarget.bitmap(fui::Rect{static_cast<int16_t>(button.x + (button.width - HEADER_ICON_SIZE) / 2),
                              static_cast<int16_t>(headerBottom - HEADER_CONTENT_BOTTOM_GAP - HEADER_ICON_SIZE),
                              HEADER_ICON_SIZE, HEADER_ICON_SIZE},
                    fui::bitmapFromIcon(icon_home_24), fui::BitmapMode::Center);
  }
}

bool FrontlightPanelActivity::showsBookProgress() const {
  return context.activeReaderBook && !context.showReaderDetails && !context.bookDetails.title.empty();
}

void FrontlightPanelActivity::drawBookProgress() {
  if (!showsBookProgress()) return;
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int y = metrics.topPadding + TouchHeaderBackButton::height(metrics, mappedInput);
  const Rect row{0, y, renderer.getScreenWidth(), metrics.tabBarHeight};
  char progress[96];
  formatReaderBookProgress(progress, sizeof(progress), context.bookDetails.chapterPage,
                           context.bookDetails.chapterPageCount, context.bookDetails.chapterPageCountEstimated,
                           context.bookDetails.progressPercent);
  GUI.drawSubHeader(renderer, row, progress);
  renderer.drawLine(row.x, row.y + row.height - 1, row.x + row.width - 1, row.y + row.height - 1, 1, true);
}

void FrontlightPanelActivity::addStepSlider(UiApp::ScreenType& screen, const fui::Rect& row, const uint8_t value,
                                            const fui::ActionId sliderAction, const fui::ActionId stepAction) {
  const auto& theme = screen.theme();
  const int16_t stepWidth = row.height;
  const fui::Rect minusHit{row.x, row.y, stepWidth, row.height};
  const fui::Rect plusHit{static_cast<int16_t>(row.right() - stepWidth), row.y, stepWidth, row.height};

  fui::TextStyle glyph = theme.bodyText;
  glyph.align = fui::TextAlign::Center;
  const int16_t lineHeight = screen.target().lineHeight(glyph.font);
  const int16_t glyphY = static_cast<int16_t>(row.y + (row.height - lineHeight) / 2);
  screen.target().text(fui::Rect{minusHit.x, glyphY, stepWidth, lineHeight}, "-", glyph);
  screen.target().text(fui::Rect{plusHit.x, glyphY, stepWidth, lineHeight}, "+", glyph);
  screen.frame().hit(minusHit, stepAction, -1, fui::InputTouch);
  screen.frame().hit(plusHit, stepAction, +1, fui::InputTouch);

  fui::SliderProps slider;
  slider.value = value;
  slider.max = 100;
  slider.action = sliderAction;
  slider.inputMask = fui::InputTouch | fui::InputDrag;
  const int16_t sideGap = static_cast<int16_t>(stepWidth + theme.spaceSm);
  fui::slider(screen.frame(), row.inset(fui::Insets{0, sideGap, 0, sideGap}), slider);
}

void FrontlightPanelActivity::render(RenderLock&&) {
  // Overlay drop-down: keep the framebuffer content (the reader/menu we opened
  // over) intact below the panel; only the top third is repainted. No full
  // clearScreen — same as the theme popups draw over the current frame.
  panelBottom = computePanelBottom();
  uiReady = false;
  if (!context.showReaderDetails) {
    // Status row (clock, battery) on white, then the card below it. The theme
    // header's title lane and rule fall under the card's backdrop and vanish.
    const int width = renderer.getScreenWidth();
    renderer.fillRect(0, 0, width, panelBottom + 6, false);
    const auto& metrics = UITheme::getInstance().getMetrics();
    GUI.drawHeader(renderer, Rect{0, metrics.topPadding, width, TouchHeaderBackButton::height(metrics, mappedInput)},
                   "", nullptr, false);
    renderer.fillRect(0, CC_CARD_TOP - 4, width, panelBottom + 10 - CC_CARD_TOP, false);
    const uint32_t frameStartMs = millis();
#ifndef SIMULATOR
    const uint32_t panelStartUs = HalDisplay::panelTimeUs();
#endif
    renderer.setSmoothRoundEnds(true);
    app.render();
    renderer.setSmoothRoundEnds(false);
    uiReady = true;
    if (optionPopup.isActive()) optionPopup.render(renderer);
    renderer.displayBuffer();
#ifndef SIMULATOR
    const auto panelMs = static_cast<int32_t>((HalDisplay::panelTimeUs() - panelStartUs) / 1000U);
#else
    const int32_t panelMs = -1;
#endif
    const uint32_t sinceInput = frameStartMs - lastInputMs;
    SpeedLog::recordNamed("cc.frame", millis() - frameStartMs,
                          lastInputMs != 0 && sinceInput < 5000 ? static_cast<int32_t>(sinceInput) : -1, panelMs);
    lastInputMs = 0;
    return;
  }
  app.render();
  uiReady = true;
  drawHeader();
  drawBookProgress();

  // The dialog is the topmost layer.
  if (optionPopup.isActive()) optionPopup.render(renderer);

  renderer.displayBuffer();
}

// ---------------------------------------------------------------------------
// VARAGH Control Center (design C): a calm card with the light and warmth
// sliders and eight buttons the reader picks in Settings > Controls.

// Buttons in use: empty slots are skipped and the rest close up, four a row.
int FrontlightPanelActivity::controlButtonCount() const {
  int count = 0;
  for (int slot = 0; slot < CrossPointSettings::CONTROL_CENTER_SLOTS; ++slot) {
    count += SETTINGS.controlCenterSlot(slot) != CrossPointSettings::CC_EMPTY ? 1 : 0;
  }
  return count;
}

int FrontlightPanelActivity::controlCenterBottom() const {
  int y = CC_CARD_TOP + CC_CARD_PAD + CC_LIGHT_H;
  if (Frontlight.hasColorTemperature()) y += CC_WARMTH_GAP + CC_WARMTH_H;
  // The card is only as tall as the rows of buttons it shows (none, one or two).
  const int rows = (controlButtonCount() + 3) / 4;
  if (rows > 0) y += CC_BUTTONS_GAP + CC_ROW_PITCH * rows - 10;  // a row's label ends 10 px before the next pitch
  return y + CC_GRABBER_BAND;
}

bool FrontlightPanelActivity::controlButtonOn(const uint8_t button) const {
  switch (button) {
    case CrossPointSettings::CC_TAP_TO_LOOK_UP:
      return SETTINGS.tapToLookUp != 0;
    case CrossPointSettings::CC_WIFI:
      return OnlineWifi::manualOn();
    case CrossPointSettings::CC_DARK:
      return SETTINGS.screenInverted != 0;
    case CrossPointSettings::CC_TOUCH_LOCK:
      return pendingTouchscreenDisabled;
    case CrossPointSettings::CC_LIGHT:
      return lightOn;
    case CrossPointSettings::CC_NIGHT_LIGHT:
      return FrontlightNight::isNight();
    case CrossPointSettings::CC_GUIDE_DOTS:
      return SETTINGS.guideReadingEnabled != 0;
    case CrossPointSettings::CC_FOCUS_READING:
      return SETTINGS.focusReadingEnabled != 0;
    default:
      return false;
  }
}

bool FrontlightPanelActivity::controlButtonEnabled(const uint8_t button) const {
  switch (button) {
    case CrossPointSettings::CC_EMPTY:
      return false;
    case CrossPointSettings::CC_READING_STATS:
      return context.showReadingStatsAction && SETTINGS.shouldTrackReadingStats();
    case CrossPointSettings::CC_ROTATE:
      return context.activeEpub;
    default:
      return true;
  }
}

// A thin track, black up to the knob and outlined after it, under a round
// white knob. Solid black and white only: no grey dots for the e-ink to blur.
void FrontlightPanelActivity::drawThinSlider(const int left, const int right, const int midY, const int knobX,
                                             const int knobD) const {
  const int top = midY - CC_TRACK_H / 2;
  renderer.drawRoundedRect(left, top, right - left, CC_TRACK_H, 2, CC_TRACK_H / 2, true);
  if (knobX > left) renderer.fillRoundedRect(left, top, knobX - left, CC_TRACK_H, CC_TRACK_H / 2, Color::Black);
  renderer.fillRoundedRect(knobX - knobD / 2, midY - knobD / 2, knobD, knobD, knobD / 2, Color::White);
  renderer.drawRoundedRect(knobX - knobD / 2, midY - knobD / 2, knobD, knobD, 2, knobD / 2, true);
}

void FrontlightPanelActivity::buildControlCenter(UiApp::ScreenType& screen) {
  auto& target = screen.target();
  const int width = renderer.getScreenWidth();
  const int x0 = CC_CARD_SIDE;
  const int x1 = width - CC_CARD_SIDE;
  const int cardBottom = panelBottom;

  // Card: a soft dotted shadow, white body, 2 px rim.
  renderer.fillRoundedRect(x0 + 3, CC_CARD_TOP + 4, x1 - x0, cardBottom - CC_CARD_TOP, CC_CARD_RADIUS,
                           Color::LightGray);
  renderer.fillRoundedRect(x0, CC_CARD_TOP, x1 - x0, cardBottom - CC_CARD_TOP, CC_CARD_RADIUS, Color::White);
  renderer.drawRoundedRect(x0, CC_CARD_TOP, x1 - x0, cardBottom - CC_CARD_TOP, 2, CC_CARD_RADIUS, true);

  // Taps outside the card close it; the grabber band closes it too.
  const int screenH = renderer.getScreenHeight();
  screen.frame().hit(fui::Rect{0, static_cast<int16_t>(cardBottom + 6), static_cast<int16_t>(width),
                               static_cast<int16_t>(screenH - cardBottom - 6)},
                     ACTION_DISMISS, 0, fui::InputTouch);
  drawerHandleRect = fui::Rect{static_cast<int16_t>(width / 2 - 60), static_cast<int16_t>(cardBottom - CC_GRABBER_BAND),
                               120, CC_GRABBER_BAND};
  screen.frame().hit(drawerHandleRect, ACTION_DISMISS, 0, fui::InputTouch);
  renderer.fillRoundedRect(width / 2 - 24, cardBottom - 14, 48, 5, 2, Color::Black);

  // Light: round - and + buttons for exact steps, between them a big
  // capsule, dark grey up to the knob, the percent in the knob.
  const int left = x0 + CC_CARD_PAD;
  const int right = x1 - CC_CARD_PAD;
  int y = CC_CARD_TOP + CC_CARD_PAD;
  const int stepCy = y + CC_LIGHT_H / 2;
  for (const int side : {-1, +1}) {
    const int bx = side < 0 ? left : right - CC_STEP_D;
    renderer.fillRoundedRect(bx, stepCy - CC_STEP_D / 2, CC_STEP_D, CC_STEP_D, CC_STEP_D / 2, Color::White);
    renderer.drawRoundedRect(bx, stepCy - CC_STEP_D / 2, CC_STEP_D, CC_STEP_D, 2, CC_STEP_D / 2, true);
    target.bitmap(fui::Rect{static_cast<int16_t>(bx + (CC_STEP_D - CC_ICON) / 2),
                            static_cast<int16_t>(stepCy - CC_ICON / 2), CC_ICON, CC_ICON},
                  fui::bitmapFromIcon(side < 0 ? icon_v_minus_24 : icon_v_plus_24), fui::BitmapMode::Center);
    screen.frame().hit(fui::Rect{static_cast<int16_t>(bx - 6), static_cast<int16_t>(y),
                                 static_cast<int16_t>(CC_STEP_D + 12), CC_LIGHT_H},
                       ACTION_BRIGHTNESS_STEP, static_cast<int16_t>(side), fui::InputTouch);
  }
  const int sliderL = left + CC_STEP_D + CC_STEP_GAP;
  const int sliderR = right - CC_STEP_D - CC_STEP_GAP;
  const int travelStart = sliderL + CC_KNOB_W / 2;
  const int travelEnd = sliderR - CC_KNOB_W / 2;
  const uint8_t shown = brightness;
  const int knobX = travelStart + (travelEnd - travelStart) * lightPermilleFromPercent(shown) / 1000;
  drawThinSlider(sliderL, sliderR, stepCy, knobX, CC_KNOB_W);
  char percent[8];
  std::snprintf(percent, sizeof(percent), "%u%%", static_cast<unsigned>(shown));
  // UI_10 bold keeps "100%" (58 px) inside the 72 px knob.
  const int textW = renderer.getTextWidth(UI_10_FONT_ID, percent, EpdFontFamily::BOLD);
  renderer.drawText(UI_10_FONT_ID, knobX - textW / 2, y + (CC_LIGHT_H - renderer.getLineHeight(UI_10_FONT_ID)) / 2,
                    percent, true, EpdFontFamily::BOLD);
  // The whole slider takes drags, the knob's overhang at either end too.
  lightSliderW = sliderR - sliderL;
  screen.frame().hit(fui::Rect{static_cast<int16_t>(sliderL), static_cast<int16_t>(y),
                               static_cast<int16_t>(lightSliderW), CC_LIGHT_H},
                     ACTION_BRIGHTNESS, 0, fui::InputTouch | fui::InputDrag);
  y += CC_LIGHT_H;

  // Warmth: - and + at the ends, then cold (snowflake) and warm (flame).
  if (Frontlight.hasColorTemperature()) {
    y += CC_WARMTH_GAP;
    const int midY = y + CC_WARMTH_H / 2;
    const int stepInset = (CC_STEP_D - CC_SMALL_STEP_D) / 2;  // centred under the light's - / +
    for (const int side : {-1, +1}) {
      const int bx = side < 0 ? left + stepInset : right - stepInset - CC_SMALL_STEP_D;
      renderer.fillRoundedRect(bx, midY - CC_SMALL_STEP_D / 2, CC_SMALL_STEP_D, CC_SMALL_STEP_D, CC_SMALL_STEP_D / 2,
                               Color::White);
      renderer.drawRoundedRect(bx, midY - CC_SMALL_STEP_D / 2, CC_SMALL_STEP_D, CC_SMALL_STEP_D, 2,
                               CC_SMALL_STEP_D / 2, true);
      target.bitmap(fui::Rect{static_cast<int16_t>(bx + (CC_SMALL_STEP_D - CC_ICON) / 2),
                              static_cast<int16_t>(midY - CC_ICON / 2), CC_ICON, CC_ICON},
                    fui::bitmapFromIcon(side < 0 ? icon_v_minus_24 : icon_v_plus_24), fui::BitmapMode::Center);
      screen.frame().hit(fui::Rect{static_cast<int16_t>(bx - 8), static_cast<int16_t>(y - 10),
                                   static_cast<int16_t>(CC_SMALL_STEP_D + 16), static_cast<int16_t>(CC_WARMTH_H + 20)},
                         ACTION_WARMTH_STEP, static_cast<int16_t>(side), fui::InputTouch);
    }
    const int coldX = left + CC_STEP_D + CC_STEP_GAP;
    const int warmX = right - CC_STEP_D - CC_STEP_GAP - CC_ICON;
    target.bitmap(fui::Rect{static_cast<int16_t>(coldX), static_cast<int16_t>(midY - CC_ICON / 2), CC_ICON, CC_ICON},
                  fui::bitmapFromIcon(icon_v_snowflake_24), fui::BitmapMode::Center);
    target.bitmap(fui::Rect{static_cast<int16_t>(warmX), static_cast<int16_t>(midY - CC_ICON / 2), CC_ICON, CC_ICON},
                  fui::bitmapFromIcon(icon_v_flame_on_24), fui::BitmapMode::Center);
    const int trackL = coldX + CC_ICON + 8;
    const int trackR = warmX - 8;
    // The knob covers each end of the track at 0 % and 100 %.
    const int wx = trackL + CC_WARM_KNOB_D / 2 + (trackR - trackL - CC_WARM_KNOB_D) * warmth / 100;
    drawThinSlider(trackL, trackR, midY, wx, CC_WARM_KNOB_D);
    warmthSliderW = trackR - trackL;
    screen.frame().hit(fui::Rect{static_cast<int16_t>(trackL), static_cast<int16_t>(y - 8),
                                 static_cast<int16_t>(trackR - trackL), static_cast<int16_t>(CC_WARMTH_H + 16)},
                       ACTION_WARMTH, 0, fui::InputTouch | fui::InputDrag);
    y += CC_WARMTH_H;
  }

  // Eight buttons in two rows: black when switched on, label underneath.
  y += CC_BUTTONS_GAP;
  const int columnW = (x1 - x0 - 24) / 4;
  int shownIndex = 0;
  for (int slot = 0; slot < CrossPointSettings::CONTROL_CENTER_SLOTS; ++slot) {
    const uint8_t button = SETTINGS.controlCenterSlot(slot);
    const bool on = controlButtonOn(button);
    const freeink::Icon* icon = controlButtonIcon(button, on);
    if (!icon) continue;
    const bool enabled = controlButtonEnabled(button);
    const int position = shownIndex++;
    const int cx = x0 + 12 + columnW * (position % 4) + columnW / 2;
    const int top = y + CC_ROW_PITCH * (position / 4);
    const int pillX = cx - CC_PILL_W / 2;
    if (on) {
      renderer.fillRoundedRect(pillX, top, CC_PILL_W, CC_PILL_H, CC_PILL_H / 2, Color::Black);
    } else {
      // Not available here (no book open): lightly dotted.
      renderer.fillRoundedRect(pillX, top, CC_PILL_W, CC_PILL_H, CC_PILL_H / 2,
                               enabled ? Color::White : Color::LightGray);
      renderer.drawRoundedRect(pillX, top, CC_PILL_W, CC_PILL_H, 1, CC_PILL_H / 2, true);
    }
    target.bitmap(fui::Rect{static_cast<int16_t>(cx - CC_ICON / 2),
                            static_cast<int16_t>(top + (CC_PILL_H - CC_ICON) / 2), CC_ICON, CC_ICON},
                  fui::bitmapFromIcon(*icon), fui::BitmapMode::Center,
                  fui::Paint::solid(on ? fui::Color::White : fui::Color::Black));
    const auto style = on ? EpdFontFamily::BOLD : EpdFontFamily::REGULAR;
    const std::string label =
        renderer.truncatedText(SMALL_FONT_ID, I18N.get(controlButtonLabel(button)), columnW - 4, style);
    const int lw = renderer.getTextWidth(SMALL_FONT_ID, label.c_str(), style);
    renderer.drawText(SMALL_FONT_ID, cx - lw / 2, top + CC_PILL_H + 6, label.c_str(), true, style);
    if (enabled) {
      screen.frame().hit(fui::Rect{static_cast<int16_t>(cx - columnW / 2), static_cast<int16_t>(top - 4),
                                   static_cast<int16_t>(columnW), static_cast<int16_t>(CC_ROW_PITCH - 8)},
                         ACTION_CC_BUTTON, static_cast<int16_t>(slot), fui::InputTouch | fui::InputLongPress);
    }
  }
}

void FrontlightPanelActivity::toggleNightLight() {
  FrontlightNight::setNight(!FrontlightNight::isNight());
  brightness = Frontlight.brightness();
  warmth = Frontlight.warmth();
  settingsDirty = true;
  requestUpdate();
}

void FrontlightPanelActivity::openWifiList() {
  // No auto-connect: hold means "show me the networks".
  auto list = makeUniqueNoThrow<WifiSelectionActivity>(renderer, mappedInput, false);
  if (!list) return;
  startActivityForResult(std::move(list), [this](const ActivityResult&) {
    if (hasActiveStationWifiConnection()) {
      // Connected from the list: keep it, as the Wi-Fi button does.
      OnlineWifi::adoptConnection();
    } else if (!OnlineWifi::manualOn() && WiFi.getMode() != WIFI_MODE_NULL) {
      WiFi.disconnect(false);
      delay(30);
      WiFi.mode(WIFI_OFF);
    }
    requestUpdate();
  });
}

void FrontlightPanelActivity::finishWithAction(const FrontlightPanelAction action) {
  FrontlightPanelResult result;
  result.state = drawerState;
  result.activeEpub = context.activeEpub;
  result.bookPath = context.bookPath;
  result.action = action;
  setResult(ActivityResult(std::move(result)));
  finish();
}

void FrontlightPanelActivity::activateControlButton(const uint8_t button, const bool longPress) {
  if (!controlButtonEnabled(button)) return;
  switch (button) {
    case CrossPointSettings::CC_TAP_TO_LOOK_UP:
      SETTINGS.tapToLookUp = SETTINGS.tapToLookUp ? 0 : 1;
      settingsDirty = true;
      requestUpdate();
      return;
    case CrossPointSettings::CC_WIFI:
      if (longPress) {
        openWifiList();
        return;
      }
      OnlineWifi::setManual(!OnlineWifi::manualOn());
      requestUpdate();
      return;
    case CrossPointSettings::CC_DARK:
      SETTINGS.screenInverted = SETTINGS.screenInverted ? 0 : 1;
      display.setInverted(SETTINGS.screenInverted != 0);
      requestUpdate();
      return;
    case CrossPointSettings::CC_TOUCH_LOCK:
      toggleReaderTouchscreen();
      return;
    case CrossPointSettings::CC_LIGHT:
      toggleLight();
      return;
    case CrossPointSettings::CC_NIGHT_LIGHT:
      toggleNightLight();
      return;
    case CrossPointSettings::CC_GUIDE_DOTS:
    case CrossPointSettings::CC_FOCUS_READING: {
      const bool dots = button == CrossPointSettings::CC_GUIDE_DOTS;
      if (context.activeEpub) {
        // A book keeps these per book: its reader toggles, saves and lays out.
        finishWithAction(dots ? FrontlightPanelAction::ToggleGuideDots : FrontlightPanelAction::ToggleFocusReading);
        return;
      }
      uint8_t& value = dots ? SETTINGS.guideReadingEnabled : SETTINGS.focusReadingEnabled;
      value = value ? 0 : 1;
      settingsDirty = true;
      requestUpdate();
      return;
    }
    case CrossPointSettings::CC_REFRESH:
      display.fullRefreshNext();
      close();
      return;
    case CrossPointSettings::CC_SYNC:
      openSyncDialog();
      return;
    case CrossPointSettings::CC_SLEEP:
      // The power button's own routine: goToSleep() alone only draws the
      // sleep screen (and replaced this panel mid-loop), it never slept.
      enterDeepSleep();
      return;
    case CrossPointSettings::CC_SETTINGS:
      openGlobalSettings();
      return;
    case CrossPointSettings::CC_READING_STATS:
      openReadingStats();
      return;
    case CrossPointSettings::CC_LIBRARY:
      activityManager.goToLibrary();
      return;
    case CrossPointSettings::CC_APPS:
      activityManager.goToApps();
      return;
    case CrossPointSettings::CC_FILE_TRANSFER:
      activityManager.goToFileTransfer(context.bookPath);
      return;
    case CrossPointSettings::CC_ROTATE:
      finishWithAction(FrontlightPanelAction::RotateRight);
      return;
    default:
      return;
  }
}
