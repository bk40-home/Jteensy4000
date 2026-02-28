/**
 * JT4000Colours.h  —  JT-4000 colour palette
 *
 * All values are standard RGB565 (RRRRRGGGGGGBBBBB).
 *
 * The ILI9341 driver sets MADCTL_BGR for all rotations.  This is a hardware
 * sub-pixel order flag that the panel applies internally — the SPI colour
 * data is still sent as standard RGB565.  DO NOT pre-swap R and B here; the
 * hardware handles that.
 *
 * Palette is matched to the JE-8086 reference image:
 *   Background  deep charcoal-navy      #10142A  0x10A4  (was 0x0821 — too warm)
 *   Header bg   dark steel-blue panel   #19233C  0x1907
 *   Text        warm off-white          #D2D7E1  0xD6BC
 *   Amber       JE-8086 LED orange      #FFA000  0xFD00
 *   Cyan        OSC section accent      #00C8FF  0x065F
 *   Scope wave  bright LCD green        #00FF3C  0x07E7
 *   Meter red   clip indicator          #FF1E1E  0xF8E3
 */

#pragma once

// ---------------------------------------------------------------------------
// Core UI colours — all standard RGB565
// ---------------------------------------------------------------------------

/** Screen background — deep charcoal-navy */
#define COLOUR_BACKGROUND   0x10A4  // #101428

/** Primary text — warm off-white */
#define COLOUR_TEXT         0xD6BC  // #D2D7E1

/** System / accent text — amber orange (JE-8086 LED colour) */
#define COLOUR_SYSTEXT      0xFD00  // #FFA000

/** Dimmed / secondary text — steel grey */
#define COLOUR_TEXT_DIM     0x7BF1  // #787D8C

/** Selected row highlight — amber (same as SYSTEXT; text on it = BACKGROUND) */
#define COLOUR_SELECTED     0xFD00  // #FFA000

/** Alert / clip indicator — red */
#define COLOUR_ACCENT       0xF8E3  // #FF1C18

/** Oscillator section labels — bright cyan */
#define COLOUR_OSC          0x065F  // #00CBFF

/** Filter section labels — warm amber-yellow */
#define COLOUR_FILTER       0xFDA5  // #FFB428

/** Envelope section labels — magenta-pink */
#define COLOUR_ENV          0xD994  // #DC32A0

/** LFO section labels — orange */
#define COLOUR_LFO          0xFB60  // #FF6C00

/** FX section labels — light cyan */
#define COLOUR_FX           0x06FF  // #00DCFF

/** Global / master labels — neutral mid-grey */
#define COLOUR_GLOBAL       0x8412  // #828291

/** Header and footer bar — dark navy-blue panel */
#define COLOUR_HEADER_BG    0x1907  // #19233C

/** Row divider / border lines — subtle blue-grey */
#define COLOUR_BORDER       0x29AA  // #2D3750

// ---------------------------------------------------------------------------
// Scope / meter colours
// ---------------------------------------------------------------------------

/** Oscilloscope waveform — bright LCD green (JE-8086 display style) */
#define COLOUR_SCOPE_WAVE   0x07E7  // #00FF38

/** Scope zero-reference line — dim green */
#define COLOUR_SCOPE_ZERO   0x0322  // #006414

/** Scope area background — near-black green tint */
#define COLOUR_SCOPE_BG     0x0060  // #000C00

/** Peak meter: safe zone */
#define COLOUR_METER_GREEN  0x16E2  // #14DC14

/** Peak meter: warning zone */
#define COLOUR_METER_YELLOW 0xFEE0  // #FFDC00

/** Peak meter: clip zone */
#define COLOUR_METER_RED    0xF8E3  // #FF1C18

// ---------------------------------------------------------------------------
// Named colours — kept for ILI9341_t3n.cpp switch-case compatibility.
// These MUST stay as standard RGB565 values — do not change.
// ---------------------------------------------------------------------------
#define RED                 0xF800  // standard RGB565 red
#define PINK                0xF81F
#define YELLOW              0xFFE0
#define GREEN               0x07E0
#define MIDDLEGREEN         0x0400
#define DARKGREEN           0x02A0
#define DX_DARKCYAN         0x030D  // switch-case label — do not change

// ---------------------------------------------------------------------------
// Grey shades (R==G==B so identical in RGB565 and BGR565)
// ---------------------------------------------------------------------------
#define GREY1               0xC618  // light grey
#define GREY2               0x52AA  // mid grey
#define GREY3               0x2104  // dark grey
#define GREY4               0x10A2  // very dark grey

// ---------------------------------------------------------------------------
// Character cell dimensions (mirrors ILI9341_t3n.h — safe to duplicate here)
// ---------------------------------------------------------------------------
#ifndef CHAR_width
  #define CHAR_width         12
  #define CHAR_height        17
  #define CHAR_width_small    6
  #define CHAR_height_small   8
#endif
