// JT4000_Sections.h
// =============================================================================
// Section table — links JE-8086 front-panel groups to UIPage page indices.
//
// A "section" is a named group of related UIPages shown as a tile on the home
// screen.  Tapping a tile opens a SectionScreen listing all pages in that group.
//
// Mapping to JE-8086 black-bordered panel groups:
//   OSC 1  → OSC1 Core / Mix+Saw / DC+Ring / Feedback
//   OSC 2  → OSC2 Core / Mix+Saw / DC+Ring / Feedback + Sources mixer
//   FILTER → Filter main / Mod / 2-pole / Xpander
//   ENV    → Amp ADSR / Filter ADSR
//   LFO    → LFO1 / LFO2 / Sync modes
//   FX     → JPFX Tone+Mod / Mod params / Delay / Reverb / Mix
//   GLOBAL → Performance / Arb waveforms / BPM clock
//
// No audio or engine dependencies — include freely.
// =============================================================================

#pragma once
#include <Arduino.h>
#include "JT4000Colours.h"

// Max UIPages per section (increase if needed)
static constexpr int SECTION_MAX_PAGES = 6;
// Total sections shown on home screen
static constexpr int SECTION_COUNT     = 7;

// -----------------------------------------------------------------------------
// SectionDef — one section entry
// -----------------------------------------------------------------------------
struct SectionDef {
    const char* label;                        // short name shown on tile (≤8 chars)
    uint16_t    colour;                       // RGB565 accent colour
    uint8_t     pages[SECTION_MAX_PAGES];     // UIPage indices (255 = unused slot)
    uint8_t     pageCount;                    // number of valid pages
};

// -----------------------------------------------------------------------------
// Section table — 7 sections covering all 26 UIPages (0..25)
// -----------------------------------------------------------------------------
static const SectionDef kSections[SECTION_COUNT] = {

    // 0 — OSC 1  (pages 0-3)
    { "OSC 1",  YELLOW,    { 0, 1, 2, 3,   255, 255 }, 4 },

    // 1 — OSC 2  (pages 4-8 inc. Sources mixer on page 8)
    { "OSC 2",  YELLOW,    { 4, 5, 6, 7, 8, 255 }, 5 },

    // 2 — FILTER (pages 9-12)
    { "FILTER", YELLOW, { 9, 10, 11, 12, 255, 255 }, 4 },

    // 3 — ENV    (pages 13-14)
    { "ENV",    YELLOW,    { 13, 14, 255, 255, 255, 255 }, 2 },

    // 4 — LFO   (pages 15-17)
    { "LFO",    YELLOW,    { 15, 16, 17, 255, 255, 255 }, 3 },

    // 5 — FX    (pages 18-22)
    { "FX",     YELLOW,     { 18, 19, 20, 21, 22, 255 }, 5 },

    // 6 — GLOBAL (pages 23-25)
    { "GLOBAL", YELLOW, { 23, 24, 25, 255, 255, 255 }, 3 },
};
