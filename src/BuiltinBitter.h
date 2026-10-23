#pragma once

// VARAGH 1.3.1: the X3 / X4 firmware leaves the Bitter bitmaps out (~920 KB) to make room for the
// Bluetooth page turner. Bitter is then the ordinary SD font "Bitter" from Download Fonts, used
// through the same path as every other installed font.
#ifndef CROSSINK_BUILTIN_BITTER
#define CROSSINK_BUILTIN_BITTER 1
#endif

// Name of the downloadable family that stands in for the built-in one.
inline constexpr char BITTER_SD_FAMILY[] = "Bitter";
