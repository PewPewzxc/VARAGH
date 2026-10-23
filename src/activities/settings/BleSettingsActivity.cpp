#include "BleSettingsActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include <cstdio>
#include <cstring>
#include <iterator>

#include "CrossPointSettings.h"
#include "MappedInputManager.h"
#include "components/TouchHeaderBackButton.h"
#include "components/UITheme.h"
#include "components/UIThemeTokens.h"
#include "components/UiAppHelpers.h"

namespace fui = freeink::ui;

namespace {
constexpr fui::ActionId ACTION_ROW = 1;
enum class MenuAction : int16_t { Enable, Scan, Paired, Mapping };
constexpr bleturner::Action MAP_ACTIONS[] = {bleturner::Action::NextPage,    bleturner::Action::PrevPage,
                                             bleturner::Action::NextChapter, bleturner::Action::PrevChapter,
                                             bleturner::Action::Confirm,     bleturner::Action::Back,
                                             bleturner::Action::ReaderMenu};
constexpr StrId MAP_LABELS[] = {StrId::STR_NEXT_PAGE,       StrId::STR_PREV_PAGE, StrId::STR_BT_NEXT_CHAPTER,
                                StrId::STR_BT_PREV_CHAPTER, StrId::STR_CONFIRM,   StrId::STR_BACK,
                                StrId::STR_BT_READER_MENU};
static_assert(std::size(MAP_ACTIONS) == std::size(MAP_LABELS));
constexpr uint32_t SCAN_MS = 15000;
constexpr unsigned long CLEAR_HOLD_MS = 700;
}  // namespace

BleSettingsActivity::BleSettingsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
    : Activity("BleSettings", renderer, mappedInput),
      uiTarget(makeUiTarget(renderer)),
      app(uiTarget, uiTarget.deviceContext()) {}

void BleSettingsActivity::onEnter() {
  Activity::onEnter();
  uiReady = false;
  applySharedUiTheme(app, uiTarget);
  app.on(ACTION_ROW, &BleSettingsActivity::onRowEvent, this);
  app.setScreen(&BleSettingsActivity::listScreen, this);
  // A saved opt-in brings the radio up here, so scanning and pairing work at once.
  if (SETTINGS.ble.enabled) bleturner::switchOn();
  refresh();
  requestUpdate();
}

void BleSettingsActivity::loop() {
  if (TouchHeaderBackButton::wasTapped(mappedInput, renderer) ||
      mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    goBack();
    return;
  }
  // Holding Confirm on a mapping row clears it (touch: a long press on the row).
  if (view == View::Mapping && mappedInput.isPressed(MappedInputManager::Button::Confirm) &&
      mappedInput.getHeldTime() >= CLEAR_HOLD_MS && learnAction == bleturner::Action::None && learnNote == nullptr) {
    clearMapping(selectedIndex);
  }
  if (uiReady) {
    const auto snapshot = touchSnapshotFrom(mappedInput);
    if (snapshot.touchPressed || snapshot.touchReleased) {
      const auto event = app.route(snapshot);
      if (app.invalidated()) requestUpdate();
      if (event) return;
    }
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm) &&
      mappedInput.getHeldTime() < CLEAR_HOLD_MS) {
    activate(selectedIndex);
    return;
  }

  const int count = static_cast<int>(rows.size());
  if (count > 0) {
    const auto move = [this, count](const int index) {
      selectedIndex = index;
      topIndex = followListSelection(selectedIndex, topIndex, visibleRows, count);
      requestUpdate();
    };
    buttonNavigator.onNextRelease([&] { move(ButtonNavigator::nextIndex(selectedIndex, count)); });
    buttonNavigator.onPreviousRelease([&] { move(ButtonNavigator::previousIndex(selectedIndex, count)); });
  }

  if (millis() - lastPollMs < 250) return;
  lastPollMs = millis();
  // A remote being paired that links becomes the chosen one: save it.
  if (SETTINGS.ble.enabled && bleturner::service()) SETTINGS.saveToFile();
  if (learnAction != bleturner::Action::None) stepLearning();
  if (signature() == shownSignature) return;  // an e-ink repaint only when something changed
  refresh();
  requestUpdate();
}

const char* BleSettingsActivity::headerTitle() const {
  switch (view) {
    case View::Menu:
      return tr(STR_BT_PAGE_TURNER);
    case View::Scan:
      return tr(STR_BT_SCAN);
    case View::Paired:
      return tr(STR_BT_PAIRED_REMOTES);
    case View::Mapping:
      return tr(STR_BT_MAP_BUTTONS);
  }
  return tr(STR_BT_PAGE_TURNER);
}

void BleSettingsActivity::goBack() {
  if (learnAction != bleturner::Action::None) {
    learnAction = bleturner::Action::None;
    learnNote = nullptr;
    refresh();
    requestUpdate();
    return;
  }
  if (view == View::Menu) {
    finish();
    return;
  }
  if (view == View::Scan) bleturner::scan(0);
  showView(View::Menu);
}

void BleSettingsActivity::showView(const View target) {
  {
    RenderLock lock(*this);
    view = target;
    learnAction = bleturner::Action::None;
    learnNote = nullptr;
    selectedIndex = 0;
    topIndex = 0;
  }
  refresh();
  requestUpdate();
}

uint32_t BleSettingsActivity::signature() const {
  const auto st = bleturner::status();
  return (st.running ? 1u : 0u) | (st.scanning ? 2u : 0u) | (st.connected ? 4u : 0u) | (st.connecting ? 8u : 0u) |
         (st.stopping ? 16u : 0u) | (SETTINGS.ble.enabled ? 32u : 0u) |
         static_cast<uint32_t>(bleturner::bondCount()) << 8 | static_cast<uint32_t>(bleturner::foundCount()) << 16;
}

void BleSettingsActivity::refresh() {
  RenderLock lock(*this);
  shownSignature = signature();
  const auto st = bleturner::status();
  char failure[48];
  if (!SETTINGS.ble.enabled || st.stopping) {
    status = tr(STR_STATE_OFF);
  } else if (bleturner::takeConnectFailure(failure, sizeof(failure))) {
    status = failure;
  } else if (!st.running) {
    status = tr(STR_BT_START_FAILED);
  } else if (st.connected) {
    status = bleturner::linked().name;
  } else if (st.connecting) {
    status = tr(STR_BT_CONNECTING_REMOTE);
  } else if (st.scanning) {
    status = tr(STR_BT_SCANNING);
  } else {
    status = tr(STR_STATE_ON);
  }

  // Labels first: the rows point into them.
  const uint8_t found = st.running ? bleturner::foundCount() : 0;
  const uint8_t bonds = st.running ? bleturner::bondCount() : 0;
  labels.clear();
  for (uint8_t i = 0; i < found; ++i) labels.emplace_back(bleturner::found(i).name);
  for (uint8_t i = 0; i < bonds; ++i) {
    const auto peer = bleturner::bond(i);
    labels.emplace_back(peer.name[0] != '\0' ? peer.name : peer.addr);
  }

  rows.clear();
  const auto add = [this](const char* label, const int16_t code, const bool enabled = true) -> fui::ListItem& {
    fui::ListItem row;
    row.label = label;
    row.actionValue = code;
    row.enabled = enabled;
    rows.push_back(row);
    return rows.back();
  };
  switch (view) {
    case View::Menu: {
      auto& enable = add(tr(STR_BT_PAGE_TURNER), static_cast<int16_t>(MenuAction::Enable));
      enable.subtitle = status.c_str();
      enable.toggle = true;
      enable.toggleChecked = SETTINGS.ble.enabled != 0;
      add(tr(STR_BT_SCAN), static_cast<int16_t>(MenuAction::Scan), st.running);
      add(tr(STR_BT_PAIRED_REMOTES), static_cast<int16_t>(MenuAction::Paired), st.running);
      auto& mapping = add(tr(STR_BT_MAP_BUTTONS), static_cast<int16_t>(MenuAction::Mapping), st.connected);
      if (!st.connected) mapping.subtitle = tr(STR_BT_CONNECT_REMOTE);
      break;
    }
    case View::Scan:
      add(st.scanning ? tr(STR_BT_STOP_SCAN) : tr(STR_BT_SCAN), -1, st.running);
      for (uint8_t i = 0; i < found; ++i) add(labels[i].c_str(), i);
      if (found == 0) add(st.scanning ? tr(STR_BT_SCANNING) : tr(STR_BT_NO_DEVICES), -1, false);
      break;
    case View::Paired:
      for (uint8_t i = 0; i < bonds; ++i) {
        auto& row = add(labels[found + i].c_str(), i);
        row.value = tr(STR_BT_FORGET);
      }
      if (bonds == 0) add(tr(STR_BT_NO_REMOTES), -1, false);
      break;
    case View::Mapping: {
      const auto peer = bleturner::linked();
      const auto* table = bleturner::tableFor(SETTINGS.ble.remotes, SETTINGS.ble.remoteCount, peer.addr, peer.name);
      for (uint8_t i = 0; i < std::size(MAP_ACTIONS); ++i) {
        auto& row = add(I18N.get(MAP_LABELS[i]), i, st.connected && learnAction == bleturner::Action::None);
        bleturner::Binding binding;
        if (table && bleturner::find(*table, MAP_ACTIONS[i], binding)) {
          bleturner::formatCode(mappingValues[i], sizeof(mappingValues[i]), binding);
          if (binding & bleturner::kHoldBit) {
            const size_t used = strlen(mappingValues[i]);
            snprintf(mappingValues[i] + used, sizeof(mappingValues[i]) - used, " %s", tr(STR_BT_HOLD));
          }
          row.value = mappingValues[i];
        } else {
          row.value = tr(STR_BT_NOT_SET);
        }
        if (!st.connected) {
          row.subtitle = tr(STR_BT_CONNECT_REMOTE);
        } else if (MAP_ACTIONS[i] == learnAction) {
          row.subtitle = tr(STR_BT_PRESS_REMOTE);
        } else if (i == 0) {
          row.subtitle = learnNote;
        }
      }
      break;
    }
  }
  if (selectedIndex >= static_cast<int>(rows.size())) selectedIndex = std::max(0, static_cast<int>(rows.size()) - 1);
}

void BleSettingsActivity::onRowEvent(const fui::ActionEvent& event, void* user) {
  auto* self = static_cast<BleSettingsActivity*>(user);
  if (event.value < 0 || event.value >= static_cast<int16_t>(self->rows.size())) return;
  self->selectedIndex = event.value;
  self->app.clearTapFlash();
  if (event.longPress) {
    if (self->view == View::Mapping) self->clearMapping(event.value);
    return;
  }
  self->activate(event.value);
}

void BleSettingsActivity::activate(const int index) {
  if (index < 0 || index >= static_cast<int>(rows.size()) || !rows[index].enabled) return;
  selectedIndex = index;
  const int16_t code = rows[index].actionValue;
  switch (view) {
    case View::Menu:
      switch (static_cast<MenuAction>(code)) {
        case MenuAction::Enable:
          SETTINGS.ble.enabled = SETTINGS.ble.enabled ? 0 : 1;
          if (SETTINGS.ble.enabled) {
            bleturner::switchOn();
          } else {
            bleturner::switchOff();
          }
          SETTINGS.saveToFile();
          break;
        case MenuAction::Scan:
          bleturner::scan(SCAN_MS);
          showView(View::Scan);
          return;
        case MenuAction::Paired:
          showView(View::Paired);
          return;
        case MenuAction::Mapping:
          showView(View::Mapping);
          return;
      }
      break;
    case View::Scan:
      if (code < 0) {
        bleturner::scan(bleturner::status().scanning ? 0 : SCAN_MS);
      } else {
        const auto peer = bleturner::found(static_cast<uint8_t>(code));
        const std::string addr = peer.addr;
        const std::string name = peer.name;
        bleturner::scan(0);
        bleturner::pair(addr.c_str(), name.c_str());
        showView(View::Menu);
        return;
      }
      break;
    case View::Paired: {
      const std::string addr = bleturner::bond(static_cast<uint8_t>(code)).addr;
      if (bleturner::forget(addr.c_str())) SETTINGS.saveToFile();
      break;
    }
    case View::Mapping:
      startLearning(code);
      break;
  }
  refresh();
  requestUpdate();
}

void BleSettingsActivity::startLearning(const int index) {
  if (index < 0 || index >= static_cast<int>(std::size(MAP_ACTIONS)) || !bleturner::status().connected) return;
  bleturner::Event ev;
  while (bleturner::pollEvent(ev)) {
  }  // presses from before the row was chosen are not the answer
  learnAction = MAP_ACTIONS[index];
  const auto peer = bleturner::linked();
  strncpy(learnAddr, peer.addr, sizeof(learnAddr) - 1);
  learnCode = 0;
  learnSinceMs = millis();
  learnNote = nullptr;
}

void BleSettingsActivity::stepLearning() {
  const auto peer = bleturner::linked();
  if (!bleturner::status().connected || strcmp(peer.addr, learnAddr) != 0) {
    learnAction = bleturner::Action::None;
    learnNote = tr(STR_BT_CONNECT_REMOTE);
    refresh();
    requestUpdate();
    return;
  }
  const auto action = learnAction;
  bleturner::Event ev;
  while (learnAction == action && bleturner::pollEvent(ev)) {
    if (ev.pressed) {
      learnCode = ev.code;
      learnPressMs = ev.atMs;
      learnSinceMs = millis();
    } else if (learnCode != 0 && ev.code == learnCode) {
      if (ev.wasRest) {
        learnCode = 0;  // the remote idling on a non-zero byte, not a button
      } else {
        // A button held past the reader's hold threshold is mapped as a hold, apart from its tap.
        keepLearned(ev.atMs - learnPressMs >= bleturner::kHoldMs);
      }
    }
  }
  if (learnAction == action) {
    // Allow the full learning window for a hold before falling back to a no-release tap.
    if (learnCode != 0 && millis() - learnSinceMs >= bleturner::kWaitMs) {
      keepLearned(false);
    } else if (learnCode == 0 && millis() - learnSinceMs >= bleturner::kWaitMs) {
      learnAction = bleturner::Action::None;
      learnNote = tr(STR_BT_NO_KEY);
    }
  }
  if (learnAction != action) {
    refresh();
    requestUpdate();
  }
}

void BleSettingsActivity::keepLearned(const bool hold) {
  // Preserve the remote's other bindings when changing the selected action.
  const auto peer = bleturner::linked();
  if (!bleturner::status().connected || strcmp(peer.addr, learnAddr) != 0) {
    learnAction = bleturner::Action::None;
    learnNote = tr(STR_BT_CONNECT_REMOTE);
    return;
  }
  bleturner::RemoteTable* table =
      bleturner::editableTable(SETTINGS.ble.remotes, SETTINGS.ble.remoteCount, peer.addr, peer.name);
  if (table == nullptr || !bleturner::learn(*table, learnAction, learnCode, hold)) {
    learnAction = bleturner::Action::None;
    learnNote = tr(STR_BT_KEYS_NOT_SAVED);
    return;
  }
  SETTINGS.saveToFile();
  learnCode = 0;
  learnSinceMs = millis();
  learnAction = bleturner::Action::None;
  learnNote = tr(STR_BT_MAPPING_SAVED);
}

void BleSettingsActivity::clearMapping(const int index) {
  if (index < 0 || index >= static_cast<int>(std::size(MAP_ACTIONS)) || learnAction != bleturner::Action::None) return;
  const auto action = MAP_ACTIONS[index];
  // The linked remote's slot for this action goes; a built-in default is copied out first, so it
  // does not come back. The page rows also drop the usage code learned before per-remote tables.
  if (bleturner::status().connected) {
    const auto peer = bleturner::linked();
    auto* table = bleturner::editableTable(SETTINGS.ble.remotes, SETTINGS.ble.remoteCount, peer.addr, peer.name);
    if (table != nullptr) bleturner::clearAction(*table, action);
  }
  if (action == bleturner::Action::NextPage) SETTINGS.ble.nextKeyUsage = bleturner::kUsageNone;
  if (action == bleturner::Action::PrevPage) SETTINGS.ble.prevKeyUsage = bleturner::kUsageNone;
  SETTINGS.saveToFile();
  learnNote = tr(STR_BT_MAPPING_CLEARED);
  refresh();
  requestUpdate();
}

void BleSettingsActivity::listScreen(UiApp::ScreenType& screen, void* user) {
  static_cast<BleSettingsActivity*>(user)->buildListScreen(screen);
}

void BleSettingsActivity::buildListScreen(UiApp::ScreenType& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  screen.setContentMargin(
      fui::Insets{static_cast<int16_t>(metrics.topPadding + TouchHeaderBackButton::height(metrics, mappedInput)), 0,
                  static_cast<int16_t>(metrics.buttonHintsHeight), 0});
  screen.spacer(static_cast<int16_t>(metrics.verticalSpacing));

  std::vector<fui::ListItem> items = rows;
  for (size_t i = 0; i < items.size(); ++i) items[i].actionValue = static_cast<int16_t>(i);  // rows dispatch by index
  fui::ListProps props;
  props.items = items.data();
  props.count = static_cast<uint16_t>(items.size());
  props.selectedIndex = static_cast<int16_t>(selectedIndex);
  props.action = ACTION_ROW;
  props.inputMask = fui::InputTouch | fui::InputLongPress;  // physical buttons stay in loop()
  props.valueInset = 8;
  props.labelText = screen.theme().bodyText;
  const auto visible = configureUiList(props, screen.theme(), screen.body(), UiListRowType::WithSubtitle);
  visibleRows = visible > 0 ? visible : 1;
  topIndex = scrollListBy(topIndex, 0, visibleRows, static_cast<int>(items.size()));
  props.topIndex = static_cast<uint16_t>(topIndex);
  screen.list(props);
}

void BleSettingsActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const Rect header = TouchHeaderBackButton::headerRect(renderer, mappedInput);
  if (mappedInput.hasTouchHardware()) {
    TouchHeaderBackButton::draw(renderer, uiTarget, header, headerTitle(), false);
  } else {
    GUI.drawHeader(renderer, header, headerTitle());
  }
  uiReady = false;
  app.render();
  uiReady = true;
  const auto labels =
      mappedInput.mapLabels(mappedInput.withBackArrow(tr(STR_BACK)), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
