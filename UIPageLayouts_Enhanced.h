/**
 * UIPageLayouts_Enhanced.h
 * 
 * Enhanced page layouts for 320x240 ILI9341 display.
 * 
 * Strategy: One page per synthesis function
 * - More parameters visible per page
 * - Logical grouping of related controls
 * - Visual parameter relationships
 * - Touch-optimized hit zones
 * 
 * Pages:
 * 0: Oscillators (OSC1 + OSC2 complete control)
 * 1: Filter & Resonance (complete filter section)
 * 2: Envelopes (Amp + Filter envelopes)
 * 3: LFOs (LFO1 + LFO2 complete control)
 * 4: Effects (Reverb, Delay, Chorus)
 * 5: Performance (Glide, Velocity, Mod wheel)
 * 6: Voice & Mix (Voice settings, oscillator mix)
 * 7: System (Presets, Tuning, Global)
 */

#pragma once
#include <Arduino.h>

namespace UIPageEnhanced {

    // Total number of pages
    static constexpr int PAGE_COUNT = 8;
    
    // Parameters per page (increased for larger display)
    static constexpr int PARAMS_PER_PAGE = 8;  // 4x2 grid

    // ========================================================================
    // PAGE 0: OSCILLATORS
    // ========================================================================
    // Complete oscillator control - waveforms, levels, tuning, modulation
    
    static const byte page0_ccs[PARAMS_PER_PAGE] = {
        CC::OSC1_WAVE,        // OSC1 Waveform
        CC::OSC1_LEVEL,       // OSC1 Level
        CC::OSC1_OCTAVE,      // OSC1 Octave
        CC::OSC1_SEMITONE,    // OSC1 Fine tune
        CC::OSC2_WAVE,        // OSC2 Waveform
        CC::OSC2_LEVEL,       // OSC2 Level
        CC::OSC2_OCTAVE,      // OSC2 Octave
        CC::OSC_DETUNE        // Detune (spread)
    };
    
    static const char* page0_names[PARAMS_PER_PAGE] = {
        "O1:Wave", "O1:Level", "O1:Oct", "O1:Semi",
        "O2:Wave", "O2:Level", "O2:Oct", "Detune"
    };

    // ========================================================================
    // PAGE 1: FILTER
    // ========================================================================
    // Complete filter section - cutoff, resonance, envelope, keyboard tracking
    
    static const byte page1_ccs[PARAMS_PER_PAGE] = {
        CC::FILTER_CUTOFF,    // Filter frequency
        CC::FILTER_RESONANCE, // Filter resonance
        CC::FILTER_ENV_AMT,   // Envelope depth
        CC::FILTER_KEY_TRACK, // Keyboard tracking
        CC::FILTER_TYPE,      // Filter type (LP/HP/BP)
        CC::FILTER_DRIVE,     // Filter drive/overdrive
        CC::FILTER_ENV_VEL,   // Envelope velocity sensitivity
        CC::LFO1_TO_FILTER    // LFO1 to filter amount
    };
    
    static const char* page1_names[PARAMS_PER_PAGE] = {
        "Cutoff", "Resonance", "Env Amt", "Key Trk",
        "Type", "Drive", "Env Vel", "LFO Amt"
    };

    // ========================================================================
    // PAGE 2: ENVELOPES
    // ========================================================================
    // Amp and Filter envelopes - ADSR for both
    
    static const byte page2_ccs[PARAMS_PER_PAGE] = {
        CC::ENV_ATTACK,       // Amp Attack
        CC::ENV_DECAY,        // Amp Decay
        CC::ENV_SUSTAIN,      // Amp Sustain
        CC::ENV_RELEASE,      // Amp Release
        CC::FILT_ENV_ATTACK,  // Filter Attack
        CC::FILT_ENV_DECAY,   // Filter Decay
        CC::FILT_ENV_SUSTAIN, // Filter Sustain
        CC::FILT_ENV_RELEASE  // Filter Release
    };
    
    static const char* page2_names[PARAMS_PER_PAGE] = {
        "Amp:Atk", "Amp:Dec", "Amp:Sus", "Amp:Rel",
        "Flt:Atk", "Flt:Dec", "Flt:Sus", "Flt:Rel"
    };

    // ========================================================================
    // PAGE 3: LFOs
    // ========================================================================
    // Complete LFO control - both LFOs with rate, depth, destination
    
    static const byte page3_ccs[PARAMS_PER_PAGE] = {
        CC::LFO1_RATE,        // LFO1 Rate
        CC::LFO1_DEPTH,       // LFO1 Depth
        CC::LFO1_WAVEFORM,    // LFO1 Waveform
        CC::LFO1_DESTINATION, // LFO1 Destination
        CC::LFO2_RATE,        // LFO2 Rate
        CC::LFO2_DEPTH,       // LFO2 Depth
        CC::LFO2_WAVEFORM,    // LFO2 Waveform
        CC::LFO2_DESTINATION  // LFO2 Destination
    };
    
    static const char* page3_names[PARAMS_PER_PAGE] = {
        "LFO1:Rate", "LFO1:Depth", "LFO1:Wave", "LFO1:Dest",
        "LFO2:Rate", "LFO2:Depth", "LFO2:Wave", "LFO2:Dest"
    };

    // ========================================================================
    // PAGE 4: EFFECTS
    // ========================================================================
    // Effects chain - reverb, delay, chorus
    
    static const byte page4_ccs[PARAMS_PER_PAGE] = {
        CC::FX_REVERB_MIX,    // Reverb mix
        CC::FX_REVERB_SIZE,   // Reverb size
        CC::FX_REVERB_DAMP,   // Reverb damping
        CC::FX_DELAY_MIX,     // Delay mix
        CC::FX_DELAY_TIME,    // Delay time
        CC::FX_DELAY_FEEDBACK,// Delay feedback
        CC::FX_CHORUS_MIX,    // Chorus mix
        CC::FX_CHORUS_RATE    // Chorus rate
    };
    
    static const char* page4_names[PARAMS_PER_PAGE] = {
        "Rev:Mix", "Rev:Size", "Rev:Damp", "Dly:Mix",
        "Dly:Time", "Dly:FB", "Chr:Mix", "Chr:Rate"
    };

    // ========================================================================
    // PAGE 5: PERFORMANCE
    // ========================================================================
    // Performance controls - glide, velocity, mod wheel, pitch bend
    
    static const byte page5_ccs[PARAMS_PER_PAGE] = {
        CC::GLIDE_TIME,       // Portamento time
        CC::GLIDE_MODE,       // Glide mode (always/legato)
        CC::VEL_TO_AMP,       // Velocity to amplitude
        CC::VEL_TO_FILTER,    // Velocity to filter
        CC::MOD_WHEEL_DEPTH,  // Mod wheel depth
        CC::MOD_WHEEL_DEST,   // Mod wheel destination
        CC::PITCH_BEND_RANGE, // Pitch bend range
        CC::AFTERTOUCH_DEPTH  // Aftertouch depth
    };
    
    static const char* page5_names[PARAMS_PER_PAGE] = {
        "Glide:Time", "Glide:Mode", "Vel>Amp", "Vel>Flt",
        "MW:Depth", "MW:Dest", "PB:Range", "AT:Depth"
    };

    // ========================================================================
    // PAGE 6: VOICE & MIX
    // ========================================================================
    // Voice settings and oscillator mixing
    
    static const byte page6_ccs[PARAMS_PER_PAGE] = {
        CC::VOICE_MODE,       // Voice mode (poly/mono/unison)
        CC::UNISON_VOICES,    // Unison voice count
        CC::UNISON_DETUNE,    // Unison detune
        CC::OSC_MIX,          // OSC1/OSC2 balance
        CC::SUB_OSC_LEVEL,    // Sub oscillator level
        CC::NOISE_LEVEL,      // Noise level
        CC::RING_MOD_LEVEL,   // Ring modulator level
        CC::MASTER_VOLUME     // Master volume
    };
    
    static const char* page6_names[PARAMS_PER_PAGE] = {
        "VoiceMode", "Uni:Vces", "Uni:Det", "Osc Mix",
        "Sub Osc", "Noise", "RingMod", "Volume"
    };

    // ========================================================================
    // PAGE 7: SYSTEM
    // ========================================================================
    // System settings - tuning, MIDI, utilities
    
    static const byte page7_ccs[PARAMS_PER_PAGE] = {
        CC::MASTER_TUNE,      // Master tuning
        CC::TRANSPOSE,        // Transpose
        CC::MIDI_CHANNEL,     // MIDI channel
        CC::BEND_RANGE,       // Pitch bend range
        CC::CLOCK_DIVIDER,    // Clock divider for sync
        CC::PRESET_BANK,      // Preset bank
        CC::PRESET_NUMBER,    // Preset number
        CC::SAVE_PRESET       // Save current preset
    };
    
    static const char* page7_names[PARAMS_PER_PAGE] = {
        "Tune", "Transp", "MIDI Ch", "Bend Rng",
        "Clk Div", "Bank", "Preset", "Save"
    };

    // ========================================================================
    // UNIFIED ARRAYS FOR EASY ACCESS
    // ========================================================================
    
    static const byte* const ccMap[PAGE_COUNT] = {
        page0_ccs,  // Oscillators
        page1_ccs,  // Filter
        page2_ccs,  // Envelopes
        page3_ccs,  // LFOs
        page4_ccs,  // Effects
        page5_ccs,  // Performance
        page6_ccs,  // Voice & Mix
        page7_ccs   // System
    };
    
    static const char* const* const ccNames[PAGE_COUNT] = {
        page0_names,
        page1_names,
        page2_names,
        page3_names,
        page4_names,
        page5_names,
        page6_names,
        page7_names
    };
    
    // Page titles for header display
    static const char* const pageTitle[PAGE_COUNT] = {
        "OSCILLATORS",
        "FILTER",
        "ENVELOPES",
        "LFOs",
        "EFFECTS",
        "PERFORMANCE",
        "VOICE & MIX",
        "SYSTEM"
    };

} // namespace UIPageEnhanced

/**
 * LAYOUT DESIGN NOTES
 * ===================
 * 
 * Display Organization (320x240):
 * 
 * +----------------------------------+
 * |  HEADER (30px)                   |
 * |  Page title, CPU, memory         |
 * +----------------------------------+
 * |                                  |
 * |  PARAMETER GRID (180px)          |
 * |  8 parameters in 4x2 layout      |
 * |  Each param: 40px high           |
 * |                                  |
 * |  [Name    ] [Val] [████████]     |
 * |  [Name    ] [Val] [████████]     |
 * |  [Name    ] [Val] [████████]     |
 * |  [Name    ] [Val] [████████]     |
 * |                                  |
 * +----------------------------------+
 * |  FOOTER (30px)                   |
 * |  Navigation hints, status        |
 * +----------------------------------+
 * 
 * Touch Zones:
 * - Direct tap on parameter row to select
 * - Swipe up/down to change pages
 * - Swipe left/right on selected param to adjust
 * - Hold parameter for fine adjustment
 * 
 * Encoder Navigation:
 * - Left encoder: Select parameter (0-7)
 * - Right encoder: Adjust value
 * - Left button SHORT: Change page
 * - Left button LONG: Toggle scope view
 * - Right button SHORT: Enter touch calibration
 * - Right button LONG: Enter menu
 */
