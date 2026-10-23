#pragma once

// VARAGH 1.3.1: what Lion gives the Bluetooth page turner (lib/BlePageTurner, from CrossPoint 2.0).
// A remote press becomes the matching key release for one main-loop pass, so every screen and the
// reader's own page-turn rules (rotation, right-to-left books, menus) treat it like a device key.
namespace blepager {

// After settings load, once.
void begin();
// Once per main-loop pass, after the input update. True when a remote press acted this pass.
bool tick();
// Right before activityManager.loop(): turns a press taken by tick() into its key release.
void applyRemoteInput();
// Before sleep: stops the radio, waiting up to timeoutMs. False when it did not stop in time.
bool beforeSleep(unsigned timeoutMs);
// The radio needs a steady clock while it is up or changing state.
bool holdsCpu();
// Section build gate: stops the radio before a chapter build (the stack holds ~50 KB). False
// when it would not let go in time; the build is then refused rather than run beside it.
bool makeRoomForChapterBuild();
// The reader's page is on screen and no build is running: a radio stopped for the build may
// start again.
void afterPagePaint();
// The radio holds the heap or is asking for it: the reader defers its look-ahead build.
bool holdsHeap();

}  // namespace blepager
