/**
 * JT4000_colours.h
 *
 * Colour constants and font-size defines required by the MicroDexed-derived
 * ILI9341_t3n.cpp display driver.
 *
 * This file replaces the `#include "config.h"` in ILI9341_t3n.cpp so the
 * driver can be used in the JT-4000 project without pulling in the entire
 * MicroDexed configuration.
 *
 * Values are identical to MicroDexed config.h (lines 248-296) — do not
 * change them without also checking ILI9341_t3n.cpp's fillSysexDataColor()
 * switch statement, which uses these as case labels.
 *
 * All colours are RGB565 (16-bit): packed as RRRRRGGGGGGBBBBB.
 */

#pragma once

// ---------------------------------------------------------------------------
// UI background / text colours (used as switch-case labels in t3n.cpp)
// ---------------------------------------------------------------------------
#define COLOR_BACKGROUND  0x0000  // Black
#define COLOR_SYSTEXT     0xFFFF  // White
#define COLOR_INSTR       0x7BBD  // Light blue
#define COLOR_CHORDS      0xE2FA  // Pale pink
#define COLOR_ARP         0xFC80  // Orange
#define COLOR_DRUMS       0xFE4F  // Salmon
#define COLOR_PITCHSMP    0x159A  // Teal

// ---------------------------------------------------------------------------
// Named colours (used as switch-case labels in t3n.cpp)
// ---------------------------------------------------------------------------
#define RED               0xD000
#define PINK              0xF81F
#define YELLOW            0xFFEB
#define GREEN             0x07E0
#define MIDDLEGREEN       0x0C00  // Slightly dark green
#define DARKGREEN         0x0AE0
#define DX_DARKCYAN       0x030D

// ---------------------------------------------------------------------------
// Grey shades
// ---------------------------------------------------------------------------
#define GREY1             0xC638  // Light grey
#define GREY2             0x52AA  // Mid grey
#define GREY3             0x2104  // Dark grey
#define GREY4             0x10A2  // Very dark grey

// ---------------------------------------------------------------------------
// Character cell dimensions (also defined in ILI9341_t3n.h — safe to repeat
// because both use the same values; the #pragma once in the .h prevents
// double-inclusion if this header is included after it)
// ---------------------------------------------------------------------------
#ifndef CHAR_width
  #define CHAR_width        12
  #define CHAR_height       17
  #define CHAR_width_small   6
  #define CHAR_height_small  8
#endif
