#pragma once

#include <FreeInkApp.h>
#include <FreeInkUIGfxRenderer.h>

#include <atomic>
#include <vector>

#include "FrontlightPanelModel.h"
#include "activities/Activity.h"
#include "components/OptionPopup.h"
#include "util/ButtonNavigator.h"

struct Rect;

// Frontlight quick panel (boards with a frontlight + home key, e.g. X4 Pro).
// Opened globally by the top-edge down-swipe: a top-anchored drop-down (only the
// upper third of the screen) with brightness and warmth sliders driving the
// light live, plus an on/off toggle with a frontlight icon. It renders as an overlay —
// the content underneath stays on screen below the panel, and a tap there
// dismisses it. State is persisted once on exit (SPIFFS write throttling).
class FrontlightPanelActivity final : public Activity {
  // Control Center: two sliders with their end targets, eight buttons, the
  // grabber and the outside-tap dismiss; reader details: a five-slot bar.
  using UiApp = freeink::ui::FreeInkApp<24, 10>;

  ButtonNavigator buttonNavigator;

  freeink::ui::GfxRendererTarget uiTarget;  // must precede `app`: the app holds a reference to it
  UiApp app;
  std::atomic<bool> uiReady{false};

  uint8_t brightness = 60;
  uint8_t warmth = 50;
  bool lightOn = false;
  FrontlightPanelContext context;
  FrontlightDrawerState drawerState{};
  OptionPopup optionPopup;
  bool initialInversion = false;
  bool initialTouchscreenDisabled = false;
  bool pendingTouchscreenDisabled = false;
  bool ttfRenderingChanged = false;
  // Swallow the swipe/tap fallout of a slider drag so its release can't
  // trigger the back gesture and close the panel mid-adjustment.
  bool draggingSlider = false;
  // Bottom edge of the drop-down (px). Content lays out above it and the
  // handle occupies its final band. Set by render() before the app lays out.
  int panelBottom = 0;
  freeink::ui::Rect drawerHandleRect{};
  std::vector<std::string> readerTitleLines;

  static void panelScreen(UiApp::ScreenType& screen, void* user);
  static void onBrightnessEvent(const freeink::ui::ActionEvent& event, void* user);
  static void onWarmthEvent(const freeink::ui::ActionEvent& event, void* user);
  static void onToggleEvent(const freeink::ui::ActionEvent& event, void* user);
  static void onBrightnessStepEvent(const freeink::ui::ActionEvent& event, void* user);
  static void onWarmthStepEvent(const freeink::ui::ActionEvent& event, void* user);
  static void onQuickActionEvent(const freeink::ui::ActionEvent& event, void* user);
  static void onDismissEvent(const freeink::ui::ActionEvent& event, void* user);
  void buildPanelScreen(UiApp::ScreenType& screen);
  void addStepSlider(UiApp::ScreenType& screen, const freeink::ui::Rect& row, uint8_t value,
                     freeink::ui::ActionId sliderAction, freeink::ui::ActionId stepAction);
  Rect homeButtonRect() const;
  // Height of the drop-down, derived from the content it holds (header +
  // sliders + toggle). Same layout math as buildPanelScreen so the frame,
  // content margin, and dismiss threshold all agree.
  int computePanelBottom() const;
  void prepareReaderDetailsLayout();
  void drawReaderDetails(UiApp::ScreenType& screen);
  // VARAGH Control Center (boards with a front light).
  static void onControlButtonEvent(const freeink::ui::ActionEvent& event, void* user);
  void drawThinSlider(int left, int right, int midY, int knobX, int knobD) const;
  int lightSliderW = 1;    // Control Center slider widths, for the drag maths
  int warmthSliderW = 1;
  bool sliderMoved = false;  // the routed drag changed a value (needs a frame)
  uint32_t lastInputMs = 0;  // for the cc.frame speed-log line
  void buildControlCenter(UiApp::ScreenType& screen);
  int controlCenterBottom() const;
  int controlButtonCount() const;
  bool controlButtonOn(uint8_t button) const;
  bool controlButtonEnabled(uint8_t button) const;
  void activateControlButton(uint8_t button, bool longPress);
  void openWifiList();
  void toggleNightLight();
  void finishWithAction(FrontlightPanelAction action);
  // A reader or global setting changed here; saved once on exit.
  bool settingsDirty = false;
  void adjustBrightness(int delta);
  void adjustWarmth(int delta);
  void toggleLight();
  void toggleReaderTouchscreen();
  void close();
  void activateQuickAction(int index);
  void openSyncDialog();
  void closeSyncDialog();
  void openReadingStats();
  void openGlobalSettings();
  void drawHeader();
  bool showsBookProgress() const;
  void drawBookProgress();

 public:
  explicit FrontlightPanelActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                   FrontlightPanelContext context = {});
  void onEnter() override;
  void onExit() override;
  // Keep the visible sliders and exit-time save in sync with a global edge action.
  void onExternalFrontlightChange() override;
  void loop() override;
  void render(RenderLock&&) override;
  // From an active reader, Home returns to the library; elsewhere it dismisses
  // the overlay back to the current screen.
  bool handleHomeGesture() override;
  bool requiresFreshBackdrop() const override { return true; }
};
