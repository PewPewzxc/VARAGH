#pragma once

#include <FontCacheManager.h>
#include <GfxRenderer.h>

#include <optional>

// Draw a screen whose text uses an SD-card font. SD fonts only draw glyphs
// that were prepared beforehand; drawing straight away turns every character
// into a replacement diamond. `draw` runs once as a scan pass (it records the
// text and draws nothing), the glyphs are loaded, then the screen is cleared
// and `draw` runs for real. Returns false, with nothing drawn, when the glyphs
// could not be prepared; the caller should then draw with a built-in font.
template <typename Draw>
bool drawWithSdFontPrewarm(GfxRenderer& renderer, const int fontId, Draw&& draw) {
  auto* fcm = renderer.getFontCacheManager();
  // A built-in font with a script fallback still draws SD glyphs.
  if (!fcm || (!renderer.isSdCardFont(fontId) && !renderer.hasScriptFallback(fontId))) {
    renderer.clearScreen();
    draw();
    return true;
  }
  // Kept alive through the real pass: its destructor clears the glyphs.
  std::optional<FontCacheManager::PrewarmScope> scope;
  const auto prepare = [&]() {
    scope.emplace(*fcm, FontCacheManager::PreparationPolicy::Normal);
    draw();
    if (scope->endScanAndPrewarm()) return true;
    scope.reset();
    return false;
  };
  if (!prepare()) {
    renderer.releaseSdCardFontForLowMemory(fontId, /*preserveAdvanceTable=*/true);
    if (!prepare()) return false;
  }
  renderer.clearScreen();
  draw();
  return true;
}
