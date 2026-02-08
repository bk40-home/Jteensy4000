#pragma once
/*
 * UIPageLayout.h
 * --------------
 * UI-only configuration mapping pages/rows to CC IDs for display + editing.
 * Do not include this file from engine or patch code.
 */

#include <Arduino.h>
#include "CCDefs.h"

namespace UIPage {

// Increase page count to accommodate all new FX parameters and the new
// arbitrary waveform selection page.  We now have
// 18 pages: the original 16 plus a Dry/Glide page and an extra page for
// ARB Bank/Table controls.
static constexpr int NUM_PAGES       = 20;
static constexpr int PARAMS_PER_PAGE = 4;

static constexpr uint8_t ccMap[NUM_PAGES][PARAMS_PER_PAGE] = {
    {CC::OSC1_WAVE,        CC::OSC1_PITCH_OFFSET, CC::OSC1_FINE_TUNE,   CC::OSC1_DETUNE},    // 0
    {CC::OSC_MIX_BALANCE,  CC::OSC1_MIX,          CC::SUPERSAW1_DETUNE, CC::SUPERSAW1_MIX},  // 1
    {CC::OSC1_FREQ_DC,     CC::OSC1_SHAPE_DC,     CC::RING1_MIX,        255},                // 2

    {CC::OSC2_WAVE,        CC::OSC2_PITCH_OFFSET, CC::OSC2_FINE_TUNE,   CC::OSC2_DETUNE},    // 3
    {CC::OSC2_MIX,         CC::SUPERSAW2_DETUNE,  CC::SUPERSAW2_MIX,    CC::NOISE_MIX},      // 4
    {CC::OSC2_FREQ_DC,     CC::OSC2_SHAPE_DC,     CC::RING2_MIX,        255},                // 5

    {CC::SUB_MIX,          255,                   255,                  255},                // 6

    {CC::FILTER_CUTOFF,    CC::FILTER_RESONANCE,  CC::FILTER_ENV_AMOUNT, CC::FILTER_KEY_TRACK}, // 7
    {CC::FILTER_OCTAVE_CONTROL, CC::FILTER_OBXA_RES_MOD_DEPTH,  CC::FILTER_OBXA_MULTIMODE, 255}, // 8
    {CC::FILTER_OBXA_TWO_POLE, CC::FILTER_OBXA_BP_BLEND_2_POLE,  CC::FILTER_OBXA_PUSH_2_POLE, 255}, // 9
    {CC::FILTER_OBXA_XPANDER_4_POLE, CC::FILTER_OBXA_XPANDER_MODE,  255, 255}, // 10

    {CC::AMP_ATTACK,       CC::AMP_DECAY,         CC::AMP_SUSTAIN,       CC::AMP_RELEASE},    // 11
    {CC::FILTER_ENV_ATTACK, CC::FILTER_ENV_DECAY, CC::FILTER_ENV_SUSTAIN, CC::FILTER_ENV_RELEASE}, // 12

    {CC::LFO1_FREQ,        CC::LFO1_DEPTH,        CC::LFO1_DESTINATION,  CC::LFO1_WAVEFORM}, // 13
    {CC::LFO2_FREQ,        CC::LFO2_DEPTH,        CC::LFO2_DESTINATION,  CC::LFO2_WAVEFORM}, // 14
    
    // Page 15: JPFX Tone + Modulation selection
    {CC::FX_BASS_GAIN, CC::FX_TREBLE_GAIN, CC::FX_MOD_EFFECT, CC::FX_MOD_MIX},    
    // Page 16: JPFX Modulation parameters
    {CC::FX_MOD_RATE, CC::FX_MOD_FEEDBACK, CC::FX_DELAY_EFFECT, CC::FX_DELAY_MIX},     
    // Page 17: JPFX Delay parameters
    {CC::FX_DELAY_FEEDBACK, CC::FX_DELAY_TIME, CC::FX_DRY_MIX, 255},
    // Page 18: Dry mix and global controls (moved Glide from original page 15)
    {255,        CC::GLIDE_ENABLE,      CC::GLIDE_TIME,         CC::AMP_MOD_FIXED_LEVEL}, // 18
    // Page 17: Arbitrary waveform bank/table selection for both oscillators
    {CC::OSC1_ARB_BANK,     CC::OSC1_ARB_INDEX,    CC::OSC2_ARB_BANK,      CC::OSC2_ARB_INDEX},     // 19
    // Note: no sentinel row; NUM_PAGES defines exactly the number of active pages.
};

static constexpr const char* ccNames[NUM_PAGES][PARAMS_PER_PAGE] = {
    {"OSC1 Wave", "OSC1 Pitch", "OSC1 Fine", "OSC1 Det"},
    {"Osc1 Mix", "Osc1 Bal" ,"SupSaw Det1", "SupSaw Mix1"},
    {"Freq DC1", "Shape DC1", "Ring Mix1", "-"},

    {"OSC2 Wave", "OSC2 Pitch", "OSC2 Fine", "OSC2 Det"},
    {"Osc2 Mix", "SupSaw Det2", "SupSaw Mix2", "Noise Mix"},
    {"Freq DC2", "Shape DC2", "Ring Mix2", "-"},

    {"Sub Mix", "-", "-", "-"},

    {"Cutoff", "Resonance", "Filt Env Amt", "Key Track"},
    {"Oct Ctrl", "Q Depth", "Multimode"},
    {"2 Pole", "Blend 2p", "Push 2p", "-"},
    {"Xpander", "XpMode", "-", "-"},
    {"Amp Att", "Amp Dec", "Amp Sus", "Amp Rel"},
    {"Filt Att", "Filt Dec", "Filt Sus", "Filt Rel"},

    {"LFO1 Freq", "LFO1 Depth", "LFO1 Dest", "LFO1 Wave"},
    {"LFO2 Freq", "LFO2 Depth", "LFO2 Dest", "LFO2 Wave"},

    {"Bass", "Treble", "Mod FX", "Mod Mix"},
    {"Mod Rate", "Mod FB", "Dly FX", "Dly Mix"},
    {"Dly FB", "Dly Time", "Dry Mix", "-"},
    {"-", "Glide En", "Glide Time", "Amp Mod DC"},
    {"OSC1 Bank", "OSC1 Table", "OSC2 Bank", "OSC2 Table"},
    // No sentinel row; the last page (ARB bank/table) is fully used.
};

} // namespace UIPage
