/**
 * JT4000Colours.h
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
#define COLOUR_BACKGROUND  0x0000  // Black



    #define COLOUR_TEXT          0xFD20   // white
    #define COLOUR_SYSTEXT       0xFFE0   // yellow  — filter params
    #define COLOUR_TEXT_DIM      0x7BEF   // mid-grey
    #define COLOUR_SELECTED      0x07FF   // cyan  — selected row highlight
    #define COLOUR_ACCENT        0xF800   // red   — alerts / clip indicator
    #define COLOUR_OSC           0x07E0   // green   — oscillator params
    #define COLOUR_FILTER        0xFFE0   // yellow  — filter params
    #define COLOUR_ENV           0xF81F   // magenta — envelope params
    #define COLOUR_LFO           0xFD20   // orange  — LFO params
    #define COLOUR_FX            0x07FF   // cyan    — FX params
    #define COLOUR_GLOBAL        0x0000   // light grey — global params
    #define COLOUR_HEADER_BG     0x4208   // dark grey  — header/footer bg
    #define COLOUR_BORDER        0x2104   // darker grey — separators

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
