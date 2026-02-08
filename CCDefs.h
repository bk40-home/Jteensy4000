/*
 * CCDefs.h - CORRECTED VERSION
 *
 * Fixed CC number conflicts. JPFX moved to 99-109 to avoid overlap with Supersaw (77-80).
 */

#pragma once

#include <Arduino.h>

namespace CC {

    // -------------------------------------------------------------------------
    // Oscillator waveforms
    // -------------------------------------------------------------------------
    static constexpr uint8_t OSC1_WAVE        = 21;
    static constexpr uint8_t OSC2_WAVE        = 22;

    // -------------------------------------------------------------------------
    // Filter main controls
    // -------------------------------------------------------------------------
    static constexpr uint8_t FILTER_CUTOFF    = 23;
    static constexpr uint8_t FILTER_RESONANCE = 24;

    // -------------------------------------------------------------------------
    // Amplifier envelope (VCA)
    // -------------------------------------------------------------------------
    static constexpr uint8_t AMP_ATTACK  = 25;
    static constexpr uint8_t AMP_DECAY   = 26;
    static constexpr uint8_t AMP_SUSTAIN = 27;
    static constexpr uint8_t AMP_RELEASE = 28;

    // -------------------------------------------------------------------------
    // Filter envelope (VCF)
    // -------------------------------------------------------------------------
    static constexpr uint8_t FILTER_ENV_ATTACK  = 29;
    static constexpr uint8_t FILTER_ENV_DECAY   = 30;
    static constexpr uint8_t FILTER_ENV_SUSTAIN = 31;
    static constexpr uint8_t FILTER_ENV_RELEASE = 32;

    // -------------------------------------------------------------------------
    // Oscillator pitch/coarse/fine/detune
    // -------------------------------------------------------------------------
    static constexpr uint8_t OSC1_PITCH_OFFSET = 41;
    static constexpr uint8_t OSC2_PITCH_OFFSET = 42;
    static constexpr uint8_t OSC1_DETUNE       = 43;
    static constexpr uint8_t OSC2_DETUNE       = 44;
    static constexpr uint8_t OSC1_FINE_TUNE    = 45;
    static constexpr uint8_t OSC2_FINE_TUNE    = 46;

    // -------------------------------------------------------------------------
    // Oscillator mix/balance and supersaw parameters
    // -------------------------------------------------------------------------
    static constexpr uint8_t OSC_MIX_BALANCE   = 47;
    static constexpr uint8_t OSC1_MIX          = 60;
    static constexpr uint8_t OSC2_MIX          = 61;
    static constexpr uint8_t SUPERSAW1_DETUNE  = 77;  // Keep existing
    static constexpr uint8_t SUPERSAW1_MIX     = 78;  // Keep existing
    static constexpr uint8_t SUPERSAW2_DETUNE  = 79;  // Keep existing
    static constexpr uint8_t SUPERSAW2_MIX     = 80;  // Keep existing
    static constexpr uint8_t SUB_MIX           = 58;
    static constexpr uint8_t NOISE_MIX         = 59;

    // -------------------------------------------------------------------------
    // Filter modulation and key tracking
    // -------------------------------------------------------------------------
    static constexpr uint8_t FILTER_ENV_AMOUNT  = 48;
    static constexpr uint8_t FILTER_KEY_TRACK   = 50;
    static constexpr uint8_t FILTER_OCTAVE_CONTROL = 84;

    // -------------------------------------------------------------------------
    // Low frequency oscillators (LFO)
    // -------------------------------------------------------------------------
    static constexpr uint8_t LFO2_FREQ        = 51;
    static constexpr uint8_t LFO2_DEPTH       = 52;
    static constexpr uint8_t LFO2_DESTINATION = 53;
    static constexpr uint8_t LFO1_FREQ        = 54;
    static constexpr uint8_t LFO1_DEPTH       = 55;
    static constexpr uint8_t LFO1_DESTINATION = 56;
    static constexpr uint8_t LFO1_WAVEFORM    = 62;
    static constexpr uint8_t LFO2_WAVEFORM    = 63;

    // -------------------------------------------------------------------------
    // Effects (hexefx/JPFX) - REORGANIZED TO AVOID CONFLICTS
    // -------------------------------------------------------------------------
    // OLD hexefx CCs (keeping for compatibility if you want to keep hexefx)
    static constexpr uint8_t FX_REVERB_SIZE     = 70;
    static constexpr uint8_t FX_REVERB_DAMP     = 71;
    static constexpr uint8_t FX_DELAY_TIME      = 72;
    static constexpr uint8_t FX_DELAY_FEEDBACK  = 73;
    static constexpr uint8_t FX_DRY_MIX         = 74;
    static constexpr uint8_t FX_REVERB_MIX      = 75;
    static constexpr uint8_t FX_DELAY_MIX       = 76;
    
    // hexefx extended parameters (from your existing code)
    static constexpr uint8_t FX_REVERB_LODAMP    = 93;
    static constexpr uint8_t FX_DELAY_MOD_RATE   = 94;
    static constexpr uint8_t FX_DELAY_MOD_DEPTH  = 95;
    static constexpr uint8_t FX_DELAY_INERTIA    = 96;
    static constexpr uint8_t FX_DELAY_TREBLE     = 97;
    static constexpr uint8_t FX_DELAY_BASS       = 98;

    // -------------------------------------------------------------------------
    // JPFX Effects (JP-8000 style) - MOVED TO 99-109 TO AVOID CONFLICTS
    // -------------------------------------------------------------------------
    static constexpr uint8_t FX_BASS_GAIN        = 99;   // Bass shelf filter
    static constexpr uint8_t FX_TREBLE_GAIN      = 100;  // Treble shelf filter
    static constexpr uint8_t FX_MOD_EFFECT       = 103;  // Modulation variation (0..10)
    static constexpr uint8_t FX_MOD_MIX          = 104;  // Mod wet/dry mix (0..1)
    static constexpr uint8_t FX_MOD_RATE         = 105;  // Mod LFO rate (Hz)
    static constexpr uint8_t FX_MOD_FEEDBACK     = 106;  // Mod feedback (0..0.99)
    static constexpr uint8_t FX_JPFX_DELAY_EFFECT   = 107;  // JPFX Delay variation (0..4)
    static constexpr uint8_t FX_JPFX_DELAY_MIX      = 108;  // JPFX Delay wet/dry mix
    static constexpr uint8_t FX_JPFX_DELAY_FEEDBACK = 109;  // JPFX Delay feedback
    static constexpr uint8_t FX_JPFX_DELAY_TIME     = 110;  // JPFX Delay time (ms)

    // -------------------------------------------------------------------------
    // Glide and global modulation
    // -------------------------------------------------------------------------
    static constexpr uint8_t GLIDE_ENABLE      = 81;
    static constexpr uint8_t GLIDE_TIME        = 82;
    static constexpr uint8_t AMP_MOD_FIXED_LEVEL = 90;

    // -------------------------------------------------------------------------
    // Arbitrary waveform selection
    // -------------------------------------------------------------------------
    static constexpr uint8_t OSC1_ARB_INDEX = 83;
    static constexpr uint8_t OSC2_ARB_INDEX = 85;
    static constexpr uint8_t OSC1_ARB_BANK  = 101;
    static constexpr uint8_t OSC2_ARB_BANK  = 102;

    // -------------------------------------------------------------------------
    // Miscellaneous synth controls
    // -------------------------------------------------------------------------
    static constexpr uint8_t OSC1_FREQ_DC      = 86;
    static constexpr uint8_t OSC1_SHAPE_DC     = 87;
    static constexpr uint8_t OSC2_FREQ_DC      = 88;
    static constexpr uint8_t OSC2_SHAPE_DC     = 89;
    static constexpr uint8_t RING1_MIX         = 91;
    static constexpr uint8_t RING2_MIX         = 92;

    // -------------------------------------------------------------------------
    // OBXa filter extended controls
    // -------------------------------------------------------------------------
    static constexpr uint8_t FILTER_OBXA_MULTIMODE = 111;
    static constexpr uint8_t FILTER_OBXA_TWO_POLE = 112;
    static constexpr uint8_t FILTER_OBXA_XPANDER_4_POLE = 113;
    static constexpr uint8_t FILTER_OBXA_XPANDER_MODE = 114;
    static constexpr uint8_t FILTER_OBXA_BP_BLEND_2_POLE = 115;
    static constexpr uint8_t FILTER_OBXA_PUSH_2_POLE = 116;
    static constexpr uint8_t FILTER_OBXA_RES_MOD_DEPTH = 117;

    // -------------------------------------------------------------------------
    // Utility: return human-readable name for a CC
    // -------------------------------------------------------------------------
    inline const char* name(uint8_t cc) {
        switch (cc) {
            case OSC1_WAVE:           return "OSC1 Wave";
            case OSC2_WAVE:           return "OSC2 Wave";
            case FILTER_CUTOFF:       return "Filter Cutoff";
            case FILTER_RESONANCE:    return "Filter Resonance";
            case AMP_ATTACK:          return "Amp Attack";
            case AMP_DECAY:           return "Amp Decay";
            case AMP_SUSTAIN:         return "Amp Sustain";
            case AMP_RELEASE:         return "Amp Release";
            case FILTER_ENV_ATTACK:   return "Filter Env Attack";
            case FILTER_ENV_DECAY:    return "Filter Env Decay";
            case FILTER_ENV_SUSTAIN:  return "Filter Env Sustain";
            case FILTER_ENV_RELEASE:  return "Filter Env Release";
            case OSC1_PITCH_OFFSET:   return "OSC1 Coarse";
            case OSC2_PITCH_OFFSET:   return "OSC2 Coarse";
            case OSC1_DETUNE:         return "OSC1 Detune";
            case OSC2_DETUNE:         return "OSC2 Detune";
            case OSC1_FINE_TUNE:      return "OSC1 Fine";
            case OSC2_FINE_TUNE:      return "OSC2 Fine";
            case OSC_MIX_BALANCE:     return "Osc Mix";
            case OSC1_MIX:            return "Osc1 Mix";
            case OSC2_MIX:            return "Osc2 Mix";
            case SUPERSAW1_DETUNE:    return "SupSaw Det1";
            case SUPERSAW1_MIX:       return "SupSaw Mix1";
            case SUPERSAW2_DETUNE:    return "SupSaw Det2";
            case SUPERSAW2_MIX:       return "SupSaw Mix2";
            case SUB_MIX:             return "Sub Mix";
            case NOISE_MIX:           return "Noise Mix";
            case FILTER_ENV_AMOUNT:   return "Filt Env Amt";
            case FILTER_KEY_TRACK:    return "Key Track";
            case FILTER_OCTAVE_CONTROL: return "Oct Ctrl";
            case LFO2_FREQ:           return "LFO2 Freq";
            case LFO2_DEPTH:          return "LFO2 Depth";
            case LFO2_DESTINATION:    return "LFO2 Dest";
            case LFO1_FREQ:           return "LFO1 Freq";
            case LFO1_DEPTH:          return "LFO1 Depth";
            case LFO1_DESTINATION:    return "LFO1 Dest";
            case LFO1_WAVEFORM:       return "LFO1 Wave";
            case LFO2_WAVEFORM:       return "LFO2 Wave";
            
            // hexefx effects
            case FX_REVERB_SIZE:      return "Rev Size";
            case FX_REVERB_DAMP:      return "Rev Damp";
            case FX_DELAY_TIME:       return "Delay Time";
            case FX_DELAY_FEEDBACK:   return "Delay FB";
            case FX_DRY_MIX:          return "Dry Mix";
            case FX_REVERB_MIX:       return "Rev Mix";
            case FX_DELAY_MIX:        return "Delay Mix";
            case FX_REVERB_LODAMP:    return "Rev LoDamp";
            case FX_DELAY_MOD_RATE:   return "Dly ModRate";
            case FX_DELAY_MOD_DEPTH:  return "Dly ModDepth";
            case FX_DELAY_INERTIA:    return "Dly Inertia";
            case FX_DELAY_TREBLE:     return "Dly Treble";
            case FX_DELAY_BASS:       return "Dly Bass";
            
            // JPFX effects
            case FX_BASS_GAIN:        return "Bass";
            case FX_TREBLE_GAIN:      return "Treble";
            case FX_MOD_EFFECT:       return "Mod Effect";
            case FX_MOD_MIX:          return "Mod Mix";
            case FX_MOD_RATE:         return "Mod Rate";
            case FX_MOD_FEEDBACK:     return "Mod FB";
            case FX_JPFX_DELAY_EFFECT:   return "JPFX Dly FX";
            case FX_JPFX_DELAY_MIX:      return "JPFX Dly Mix";
            case FX_JPFX_DELAY_FEEDBACK: return "JPFX Dly FB";
            case FX_JPFX_DELAY_TIME:     return "JPFX Dly Time";
            
            case GLIDE_ENABLE:        return "Glide En";
            case GLIDE_TIME:          return "Glide Time";
            case AMP_MOD_FIXED_LEVEL: return "Amp Mod DC";
            case OSC1_FREQ_DC:        return "Osc1 Freq DC";
            case OSC1_SHAPE_DC:       return "Osc1 Shape DC";
            case OSC2_FREQ_DC:        return "Osc2 Freq DC";
            case OSC2_SHAPE_DC:       return "Osc2 Shape DC";
            case RING1_MIX:           return "Ring Mix1";
            case RING2_MIX:           return "Ring Mix2";
            case OSC1_ARB_BANK:       return "Osc1 Bank";
            case OSC2_ARB_BANK:       return "Osc2 Bank";
            case OSC1_ARB_INDEX:      return "Osc1 Table";
            case OSC2_ARB_INDEX:      return "Osc2 Table";

            default:                  return nullptr;
        }
    }

} // namespace CC