#pragma once

#include <string>

class GfxRenderer;

// The release codename's mark: a 21x17 pixel-art face shown after the version
// and codename, e.g. "1.3.0 Lion" with a lion face (Settings > System footer and
// the boot screen). VARAGH 1.1.0-1.2.1 were "Tiger" with a tiger face.
// The full version string stays in CROSSINK_VERSION for
// update checks, the update screen, the web portal and diagnostics.
namespace CodenameMark {

constexpr int WIDTH = 21;
constexpr int HEIGHT = 17;
// Space between the label text and the face.
constexpr int LABEL_GAP = 6;

// "<version> <codename>", e.g. "1.3.0 Lion": CROSSINK_VERSION without its build
// suffix (such as "-x4-pro" or "-dev+branch") followed by the codename.
std::string versionLabel();

// Draw the face with its top-left corner at logical (x, y). Only ink pixels
// are written, so it sits on any background.
void draw(const GfxRenderer& renderer, int x, int y, bool black = true);

// Width of "<label>" plus the gap and the face.
int labelWidth(const GfxRenderer& renderer, int fontId, const char* label);

// Draw "<label>" followed by the face at logical (x, y), where y is the top of
// the text line as in GfxRenderer::drawText. The face is centred on the text.
void drawLabel(const GfxRenderer& renderer, int fontId, int x, int y, const char* label, bool black = true);

// Same, centred horizontally across pageWidth.
void drawLabelCentered(const GfxRenderer& renderer, int fontId, int pageWidth, int y, const char* label,
                       bool black = true);

}  // namespace CodenameMark
