#pragma once

#include "GameUi.h"
#include "activities/Activity.h"
#include "games/WordleGame.h"

// Touch Wordle: six rows of letter tiles over a QWERTY keyboard whose keys
// carry what is known about each letter. A tap on a tile of the row being
// typed moves the marker there, so the letters can be entered in any order.
// Give up, in the header, has to be held. When a round ends the keyboard gives
// way to the result and the statistics. The round is saved, so leaving the
// screen (or the reader going to sleep) never loses it.
class WordleActivity final : public Activity {
 public:
  WordleActivity(GfxRenderer& renderer, MappedInputManager& mappedInput);

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

  static constexpr char kSavePath[] = "/.crosspoint/games/wordle.bin";

 private:
  // 26 letters, then Enter and Backspace.
  static constexpr int kKeyCount = 28;
  static constexpr int kEnterKey = 26;
  static constexpr int kBackspaceKey = 27;

  enum class Notice : uint8_t { None, TooShort, NotAWord, NoDictionary };

  void layout();
  void startRound();
  void pressKey(int key);
  void save();
  void lookUpAnswer();
  // activeRow: a tile of the row being typed; marked: the tile the next letter goes into.
  void drawTile(const gameui::Box& box, char letter, wordle::Mark mark, bool activeRow, bool marked) const;
  void drawKeyboard() const;
  void drawResult() const;

  wordle::Game game;
  Notice notice = Notice::None;
  bool dirty = false;  // round changed since the last save

  gameui::Box tiles[wordle::kMaxGuesses][wordle::kWordLength];
  gameui::Box keys[kKeyCount];
  gameui::Box noticeBox;
  gameui::HoldButton giveUpHold;
  gameui::Box panelBox;  // result area that replaces the keyboard
  gameui::Box lookUpBox;
  gameui::Box newGameBox;
};
