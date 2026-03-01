/**
 * JT4000Colours.h  —  JT-4000 colour palette
 *
 * WHY THE VALUES LOOK "WRONG"
 * ===========================
 * The ILI9341 driver sets MADCTL_BGR (bit 3) for every rotation.
 * Per the ILI9341 datasheet (p.127) this flag swaps the RED and BLUE
 * colour channels in the panel hardware:
 *
 *   Bit positions [15:11] (normally R in RGB565) → drive the BLUE sub-pixel
 *   Bit positions [4:0]   (normally B in RGB565) → drive the RED  sub-pixel
 *
 * Consequence: to display the colour you want, you must SEND a value with
 * R and B pre-swapped.  The formula is:
 *
 *   send = ((B>>3) << 11) | ((G>>2) << 5) | (R>>3)   // BGR565 for BGR panel
 *
 * Example — to display amber #FFA000 (R=255, G=160, B=0):
 *   send = ((0>>3)<<11) | ((160>>2)<<5) | (255>>3) = 0x051F
 *
 * Every value in this file has been computed using that formula.
 * DO NOT change them to standard RGB565 — the panel will show the wrong colour.
 */

#pragma once

// ---------------------------------------------------------------------------
// Core UI colours  (BGR565 pre-swapped for ILI9341 BGR panel)
// Comments show the intended DISPLAY colour in standard #RRGGBB hex.
// ---------------------------------------------------------------------------

#define COLOUR_BACKGROUND   0x28A2  // display: #101428 deep charcoal-navy
#define COLOUR_TEXT         0xE6BA  // display: #D2D7E1 warm off-white
#define COLOUR_SYSTEXT      0x051F  // display: #FFA000 amber orange (JE-8086 LED)
#define COLOUR_TEXT_DIM     0x8BEF  // display: #787D8C steel grey
#define COLOUR_SELECTED     0x051F  // display: #FFA000 amber (selected row bg)
#define COLOUR_ACCENT       0x18FF  // display: #FF1C18 red
#define COLOUR_OSC          0xFE40  // display: #00CBFF bright cyan
#define COLOUR_FILTER       0x2DBF  // display: #FFB428 warm amber-yellow
#define COLOUR_ENV          0xA19B  // display: #DC32A0 magenta-pink
#define COLOUR_LFO          0x037F  // display: #FF6C00 orange
#define COLOUR_FX           0xFEE0  // display: #00DCFF light cyan
#define COLOUR_GLOBAL       0x9410  // display: #828291 neutral grey
#define COLOUR_HEADER_BG    0x3903  // display: #19233C dark navy panel
#define COLOUR_BORDER       0x51A5  // display: #2D3750 blue-grey

// ---------------------------------------------------------------------------
// Scope / meter colours  (BGR565 pre-swapped)
// ---------------------------------------------------------------------------
#define COLOUR_SCOPE_WAVE   0x3FE0  // display: #00FF38 bright LCD green
#define COLOUR_SCOPE_ZERO   0x1320  // display: #006414 dim green
#define COLOUR_SCOPE_BG     0x0060  // display: #000C00 near-black green tint
#define COLOUR_METER_GREEN  0x16E2  // display: #14DC14 bright green
#define COLOUR_METER_YELLOW 0x06FF  // display: #FFDC00 yellow
#define COLOUR_METER_RED    0x18FF  // display: #FF1C18 red

// ---------------------------------------------------------------------------
// Named colours — used internally by ILI9341_t3n driver switch-cases.
// These are standard RGB565; do not change them.
// ---------------------------------------------------------------------------
#define RED                 0xF800
#define PINK                0xF81F
#define YELLOW              0xFFE0
#define GREEN               0x07E0
#define MIDDLEGREEN         0x0400
#define DARKGREEN           0x02A0
#define DX_DARKCYAN         0x030D

// Grey shades — R==G==B so identical in RGB565 and BGR565
#define GREY1               0xC618
#define GREY2               0x52AA
#define GREY3               0x2104
#define GREY4               0x10A2

// Character cell dimensions (mirrors ILI9341_t3n.h)
#ifndef CHAR_width
  #define CHAR_width         12
  #define CHAR_height        17
  #define CHAR_width_small    6
  #define CHAR_height_small   8
#endif
