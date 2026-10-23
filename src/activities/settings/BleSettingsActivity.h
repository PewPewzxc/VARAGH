#pragma once

#include <BlePageTurner.h>
#include <FreeInkApp.h>
#include <FreeInkUIGfxRenderer.h>

#include <atomic>
#include <string>
#include <vector>

#include "activities/Activity.h"
#include "util/ButtonNavigator.h"

// VARAGH 1.3.1, from CrossPoint 2.0: the Bluetooth page turner's screen. Switch it on, scan, pair
// a remote, forget one, and map its buttons. The radio and the remote belong to lib/BlePageTurner;
// this screen only asks it.
class BleSettingsActivity final : public Activity {
  using UiApp = freeink::ui::FreeInkApp<24, 4>;

 public:
  BleSettingsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput);

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  enum class View { Menu, Scan, Paired, Mapping };
  View view = View::Menu;

  ButtonNavigator buttonNavigator;
  freeink::ui::GfxRendererTarget uiTarget;  // must precede `app`: the app holds a reference to it
  UiApp app;
  std::atomic<bool> uiReady{false};
  int selectedIndex = 0;
  int visibleRows = 1;
  int topIndex = 0;

  // Written on the main loop under the render lock, read by the screen builder on the render task.
  std::vector<freeink::ui::ListItem> rows;
  std::vector<std::string> labels;
  std::string status;
  uint32_t shownSignature = 0;
  unsigned long lastPollMs = 0;
  bleturner::Action learnAction = bleturner::Action::None;
  char learnAddr[18]{};
  char mappingValues[8][24]{};
  uint32_t learnCode = 0;     // the pressed button, kept until its release
  uint32_t learnPressMs = 0;  // when it went down (the remote's clock), to tell a hold from a tap
  unsigned long learnSinceMs = 0;
  const char* learnNote = nullptr;

  const char* headerTitle() const;
  void goBack();
  void showView(View target);
  uint32_t signature() const;
  void refresh();
  void activate(int index);
  void startLearning(int index);
  void stepLearning();
  void keepLearned(bool hold);
  void clearMapping(int index);

  static void listScreen(UiApp::ScreenType& screen, void* user);
  static void onRowEvent(const freeink::ui::ActionEvent& event, void* user);
  void buildListScreen(UiApp::ScreenType& screen);
};
