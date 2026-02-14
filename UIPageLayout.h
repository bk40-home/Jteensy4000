/*
 * UIPageLayout.h - WITH BPM TIMING PAGE
 * 
 * FX pages logically grouped + BPM timing controls:
 * - Page 15: JPFX Tone + Modulation
 * - Page 16: JPFX Delay
 * - Page 17: Reverb Controls
 * - Page 18: FX Mix Levels
 * - Page 19: Global settings
 * - Page 20: Arbitrary Waveforms
 * - Page 21: BPM Clock & Timing (NEW)
 */

#pragma once

#include <Arduino.h>
#include "CCDefs.h"

namespace UIPage {

static constexpr int NUM_PAGES       = 24;
static constexpr int PARAMS_PER_PAGE = 4;

static constexpr uint8_t ccMap[NUM_PAGES][PARAMS_PER_PAGE] = {
    // =========================================================================
    // OSCILLATOR PAGES (0-6)
    // =========================================================================
    
    // Page 0-3: OSC1
    {CC::OSC1_WAVE,        CC::OSC1_PITCH_OFFSET, CC::OSC1_FINE_TUNE,   CC::OSC1_DETUNE},
    {CC::OSC_MIX_BALANCE,   CC::SUPERSAW1_DETUNE, CC::SUPERSAW1_MIX,   255},
    {CC::OSC1_FREQ_DC,     CC::OSC1_SHAPE_DC,     CC::RING1_MIX,        255},
    {CC::OSC1_FEEDBACK_AMOUNT,     CC::OSC1_FEEDBACK_MIX,     255,        255},

    // Page 4-6: OSC2
    {CC::OSC2_WAVE,        CC::OSC2_PITCH_OFFSET, CC::OSC2_FINE_TUNE,   CC::OSC2_DETUNE},
    {CC::OSC2_FREQ_DC,     CC::OSC2_SHAPE_DC,     CC::RING2_MIX,        255},
    {CC::OSC1_FEEDBACK_AMOUNT,     CC::OSC2_FEEDBACK_MIX,     255,        255},

    // Page 6: Sub Oscillator
    {CC::SUB_MIX,CC::NOISE_MIX,CC::OSC1_MIX, CC::OSC2_MIX},

    // =========================================================================
    // FILTER PAGES (7-10)
    // =========================================================================
    
    {CC::FILTER_CUTOFF,              CC::FILTER_RESONANCE,           CC::FILTER_ENV_AMOUNT,        CC::FILTER_KEY_TRACK},
    {CC::FILTER_OCTAVE_CONTROL,      CC::FILTER_OBXA_RES_MOD_DEPTH,  CC::FILTER_OBXA_MULTIMODE,    255},
    {CC::FILTER_OBXA_TWO_POLE,       CC::FILTER_OBXA_BP_BLEND_2_POLE, CC::FILTER_OBXA_PUSH_2_POLE, 255},
    {CC::FILTER_OBXA_XPANDER_4_POLE, CC::FILTER_OBXA_XPANDER_MODE,   255,                          255},

    // =========================================================================
    // ENVELOPE PAGES (11-12)
    // =========================================================================
    
    {CC::AMP_ATTACK,        CC::AMP_DECAY,         CC::AMP_SUSTAIN,        CC::AMP_RELEASE},
    {CC::FILTER_ENV_ATTACK, CC::FILTER_ENV_DECAY,  CC::FILTER_ENV_SUSTAIN, CC::FILTER_ENV_RELEASE},

    // =========================================================================
    // LFO PAGES (13-14) - Updated with timing mode controls
    // =========================================================================
    
    {CC::LFO1_FREQ,        CC::LFO1_DEPTH,        CC::LFO1_DESTINATION,  CC::LFO1_TIMING_MODE},
    {CC::LFO2_FREQ,        CC::LFO2_DEPTH,        CC::LFO2_DESTINATION,  CC::LFO2_TIMING_MODE},

    // =========================================================================
    // FX PAGES (15-18)
    // =========================================================================
    
    // Page 15: JPFX Tone + Modulation Effect
    {CC::FX_BASS_GAIN,     CC::FX_TREBLE_GAIN,    CC::FX_MOD_EFFECT,     CC::FX_MOD_MIX},
    
    // Page 16: JPFX Modulation Parameters + Delay Effect Selection
    {CC::FX_MOD_RATE,      CC::FX_MOD_FEEDBACK,   CC::FX_JPFX_DELAY_EFFECT, CC::FX_JPFX_DELAY_MIX},
    
    // Page 17: JPFX Delay Parameters - Updated with timing mode
    {CC::FX_JPFX_DELAY_FEEDBACK, CC::FX_JPFX_DELAY_TIME, CC::DELAY_TIMING_MODE, 255},
    
    // Page 18: Reverb Controls
    {CC::FX_REVERB_SIZE,   CC::FX_REVERB_DAMP,    CC::FX_REVERB_LODAMP,  CC::FX_REVERB_MIX},

    // Page 19: FX Mix Levels
    {CC::FX_DRY_MIX,       CC::FX_JPFX_MIX,       CC::FX_REVERB_MIX,     255},

    // =========================================================================
    // GLOBAL PAGES (20-21)
    // =========================================================================
    
    // Page 20: Glide + Amp Modulation
    {CC::GLIDE_ENABLE,     CC::GLIDE_TIME,        CC::AMP_MOD_FIXED_LEVEL, 255},
    
    // Page 21: Arbitrary Waveform Selection
    {CC::OSC1_ARB_BANK,    CC::OSC1_ARB_INDEX,    CC::OSC2_ARB_BANK,      CC::OSC2_ARB_INDEX},

    // =========================================================================
    // BPM TIMING PAGE (22) - NEW
    // =========================================================================
    
    // Page 22: BPM Clock & Timing
    {CC::BPM_CLOCK_SOURCE, CC::BPM_INTERNAL_TEMPO, 255, 255},
};

// Display names for each parameter on each page
static constexpr const char* ccNames[NUM_PAGES][PARAMS_PER_PAGE] = {
    // =========================================================================
    // OSCILLATOR PAGES (0-6)
    // =========================================================================
    
    // OSC1
    {"OSC1 Wave",  "OSC1 Pitch", "OSC1 Fine",  "OSC1 Det"},
    {"Osc Bal",  "SSaw1 Det",   "SSaw1 Mix", "-"},
    {"Freq DC1",   "Shape DC1",  "Ring1 Mix",  "-"},
    {"OSC1 FB AMT",   "OSC1 FB MIX",  "-",  "-"},

    // OSC2
    {"OSC2 Wave",  "OSC2 Pitch", "OSC2 Fine",  "OSC2 Det"},
    {"Freq DC2",   "Shape DC2",  "Ring2 Mix",  "-"},
    {"OSC2 FB AMT",   "OSC2 FB MIX",  "-",  "-"},

    // Sub
    {"Sub Mix",     "Noise Mix",          "Osc1 Mix",          "Osc2 Mix"},

    // =========================================================================
    // FILTER PAGES (7-10)
    // =========================================================================
    
    {"Cutoff",     "Resonance",  "Flt Env Amt", "Key Track"},
    {"Oct Ctrl",   "Q Depth",    "Multimode",   "-"},
    {"2 Pole",     "Blend 2p",   "Push 2p",     "-"},
    {"Xpander",    "Xpand Mode", "-",           "-"},

    // =========================================================================
    // ENVELOPE PAGES (11-12)
    // =========================================================================
    
    {"Amp Att",    "Amp Dec",    "Amp Sus",     "Amp Rel"},
    {"Flt Att",    "Flt Dec",    "Flt Sus",     "Flt Rel"},

    // =========================================================================
    // LFO PAGES (13-14) - Updated with timing mode
    // =========================================================================
    
    {"LFO1 Freq",  "LFO1 Depth", "LFO1 Dest",   "LFO1 Sync"},
    {"LFO2 Freq",  "LFO2 Depth", "LFO2 Dest",   "LFO2 Sync"},

    // =========================================================================
    // FX PAGES (15-18)
    // =========================================================================
    
    // Page 15: Tone + Modulation Effect Type
    {"Bass",       "Treble",     "Mod FX",      "Mod Mix"},
    
    // Page 16: Modulation Parameters + Delay Type
    {"Mod Rate",   "Mod FB",     "Dly FX",      "Dly Mix"},
    
    // Page 17: Delay Parameters + Timing Mode
    {"Dly FB",     "Dly Time",   "Dly Sync",    "-"},
    
    // Page 18: Reverb
    {"Rev Size",   "Rev Damp",   "Rev LoDamp",  "Rev Mix"},

    // Page 19: Mix Levels
    {"Dry Mix",    "JPFX Mix",   "Rev Mix",     "-"},

    // =========================================================================
    // GLOBAL PAGES (20-21)
    // =========================================================================
    
    {"Glide On",   "Glide Time", "Amp Mod",     "-"},
    {"OSC1 Bank",  "OSC1 Wave#", "OSC2 Bank",   "OSC2 Wave#"},

    // =========================================================================
    // BPM TIMING PAGE (22) - NEW
    // =========================================================================
    
    {"Clock Src",  "Int BPM",    "-",           "-"},
};



} // namespace UIPage