#pragma once

#include <SdCardFontManager.h>
#include <SdCardFontRegistry.h>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <string>

#include "ReaderFontSizeStep.h"

class GfxRenderer;

struct DictionaryFontActivation {
  int fontId = 0;
  bool usingDictionaryFont = false;
};

/// Facade that owns the SD card font registry, manager, and resolver logic.
/// Hides implementation details behind a single begin() + ensureLoaded() API.
class SdCardFontSystem {
 public:
  using SettingsPersistenceCallback = void (*)(void* context);

  SdCardFontSystem() = default;
  SdCardFontSystem(const SdCardFontSystem&) = delete;
  SdCardFontSystem& operator=(const SdCardFontSystem&) = delete;
  /// Register the font resolver and load a saved SD font selection. When the
  /// built-in font is selected, discovery stays deferred until font metadata
  /// is explicitly requested.
  void begin(GfxRenderer& renderer);

  /// Ensure the correct SD font family is loaded for the current settings.
  /// Call before entering the reader or after settings change.
  /// Also re-discovers if the registry has been marked dirty (e.g. by web upload).
  void ensureLoaded(GfxRenderer& renderer);

  /// When `primaryFontId` cannot draw Persian/Arabic script or IPA, load an
  /// installed SD family that can (NotoVazir, for example) at `pointSize`
  /// (0 = the active reader size) and register it as the renderer's script
  /// fallback for that font. Words the primary cannot draw are then drawn in
  /// the fallback. X4 Pro only (it needs a second resident font). No-op when
  /// already attached, when the font covers those scripts, or when no
  /// installed family does.
  void attachScriptFallback(GfxRenderer& renderer, int primaryFontId, uint8_t pointSize = 0);

  // An EPUB can own a temporary per-book settings snapshot while this system
  // repairs a missing font selection. Let that reader persist its own state.
  void setSettingsPersistenceCallback(SettingsPersistenceCallback callback, void* context) {
    settingsPersistenceCallback_ = callback;
    settingsPersistenceContext_ = context;
  }

  /// Temporarily unload the active SD font without clearing the saved setting.
  /// Call ensureLoaded() later to restore it before reader rendering.
  void releaseLoadedFont(GfxRenderer& renderer);

  /// Release all SD-font RAM that network/TLS work does not need.
  void releaseForNetwork(GfxRenderer& renderer);

  /// Ensure the font catalog is available for settings/web enumeration, including
  /// newly uploaded or deleted fonts visible in the web UI.
  void ensureRegistry();

  /// Release catalog names and paths without unloading the active reader font.
  void releaseRegistry();

  /// Resolve an SD card font ID from family name + selected point size.
  /// Returns 0 if not found. Used by CrossPointSettings::getReaderFontId().
  int resolveFontId(const char* familyName, uint8_t pointSize) const;

  /// Change the reader font size using the active SD family when one is selected.
  bool changeReaderFontSize(bool larger, FontSizeStepMode mode = FontSizeStepMode::Wrap);

  /// Convert a pre-point-size SD font slot into the exact installed size.
  /// Used while reading legacy per-book reader settings.
  uint8_t resolveLegacySizeStep(const char* familyName, uint8_t sizeStep);

  // Temporarily replace the reader SD font with a per-book dictionary font.
  // At most one SD font family remains resident. Missing or failed dictionary
  // fonts leave the reader font active and retain the saved selection.
  // targetPointSize of zero follows the active reader size. The selected file
  // still uses the closest installed size, so a removed font file degrades
  // safely without retaining another family or size cache in RAM.
  DictionaryFontActivation activateDictionaryFont(GfxRenderer& renderer, const char* familyName,
                                                  uint8_t targetPointSize = 0);

  // Restore the saved reader font after a dictionary lookup and return its ID.
  int restoreReaderFont(GfxRenderer& renderer);

  /// True when the installed family's regular style has a glyph for every
  /// codepoint in `probes` (at most 32). Reads only one file's header and
  /// interval table; no glyph data is loaded and no font is activated.
  bool familyCovers(const char* familyName, const uint32_t* probes, size_t probeCount);

  /// First installed family (alphabetical) that covers every probe, or an
  /// empty string. Used to pick a script-capable font for a book automatically.
  std::string findFamilyCovering(const uint32_t* probes, size_t probeCount);

  /// Access the registry (e.g. for settings UI to enumerate available fonts).
  const SdCardFontRegistry& registry() const { return registry_; }

  /// Non-const access to the registry (for FontInstaller).
  SdCardFontRegistry& registry() { return registry_; }

  /// Mark the registry as needing re-discovery.
  /// Thread-safe: can be called from the web server task.
  void markRegistryDirty() {
    registryDirty_.store(true, std::memory_order_release);
    scriptFallbackLookupDone_.store(false, std::memory_order_release);
    SdCardFontRegistry::invalidateIndex();
  }
  void markRegistryDirtyForPath(const char* path);

  /// Ensure the registry is available and re-scan it after SD changes.
  /// Used by the web UI so uploaded/deleted fonts appear in the list
  /// without waiting for the reader activity to run ensureLoaded().
  void refreshIfDirty() { ensureRegistry(); }

 private:
  void persistSettingsChange() const;

  // Load the active SD family at the built-in UI point sizes and register each
  // as a size-matched CJK fallback for the corresponding UI font, so CJK book
  // titles/list rows render at the same size as the surrounding Latin UI text.
  // No-op when no SD family is loaded. Safe to call repeatedly (sizes already
  // loaded are reused).
  void setupUiFallbacks(GfxRenderer& renderer);
  void setupUiFallbacksDirect(GfxRenderer& renderer, const char* familyName);
  void ensureLoadedImpl(GfxRenderer& renderer);
  DictionaryFontActivation activateDictionaryFontImpl(GfxRenderer& renderer, const char* familyName,
                                                      uint8_t targetPointSize);
  int restoreReaderFontImpl(GfxRenderer& renderer);

  // Installed family used as the script fallback; looked up once per font
  // catalog change (empty when no installed family covers the scripts).
  std::string scriptFallbackFamily_;
  std::atomic<bool> scriptFallbackLookupDone_{false};

  SdCardFontRegistry registry_;
  SdCardFontManager manager_;
  std::atomic<bool> registryDirty_{false};
  bool registryLoaded_ = false;
  uint8_t loadedFontPointSize_ = 0;
  bool fontReloadPending_ = false;
  uint32_t loadedRegistryRevision_ = 0;
  SettingsPersistenceCallback settingsPersistenceCallback_ = nullptr;
  void* settingsPersistenceContext_ = nullptr;
};

// Global SD card font system instance (defined in main.cpp).
extern SdCardFontSystem sdFontSystem;
