/*
 * CCDefs.h
 *
 * Centralised definitions for all MIDI Control Change (CC) numbers used
 * throughout the JT‑4000 project.  Historically the project relied on raw
 * integer literals (e.g. 21, 23, 41) sprinkled through the engine, UI and
 * patch code.  This header gives each CC a meaningful name and gathers
 * them in one place.  By using these symbols instead of magic numbers
 * the intent of each mapping becomes clear, and you only need to update
 * a single definition if a CC assignment changes.
 *
 * The values here reflect the current JT‑4000 CC assignments as found
 * across CCMap.h, SynthEngine::handleControlChange, Patch.cpp and
 * UIManager.cpp.  Where multiple CCs control a similar parameter
 * (e.g. there are two controls for oscillator mix: 47 and 60) separate
 * constants are provided.  Unused or reserved CC numbers are not
 * enumerated.
 */

#pragma once

#include <Arduino.h>

namespace CC {

    // -------------------------------------------------------------------------
    // Oscillator waveforms
    // -------------------------------------------------------------------------
    static constexpr uint8_t OSC1_WAVE        = 21;  // OSC1 waveform selector
    static constexpr uint8_t OSC2_WAVE        = 22;  // OSC2 waveform selector

    // -------------------------------------------------------------------------
    // Filter main controls
    // -------------------------------------------------------------------------
    static constexpr uint8_t FILTER_CUTOFF    = 23;  // Filter cutoff frequency (log/tapered)
    static constexpr uint8_t FILTER_RESONANCE = 24;  // Filter resonance

    // -------------------------------------------------------------------------
    // Amplifier envelope (VCA)
    // -------------------------------------------------------------------------
    static constexpr uint8_t AMP_ATTACK  = 25;  // Attack time (ms)
    static constexpr uint8_t AMP_DECAY   = 26;  // Decay time (ms)
    static constexpr uint8_t AMP_SUSTAIN = 27;  // Sustain level (0–1)
    static constexpr uint8_t AMP_RELEASE = 28;  // Release time (ms)

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
    static constexpr uint8_t OSC1_PITCH_OFFSET = 41; // Coarse pitch (offset in semitones)
    static constexpr uint8_t OSC2_PITCH_OFFSET = 42;
    static constexpr uint8_t OSC1_DETUNE       = 43; // Detune amount (–1..1)
    static constexpr uint8_t OSC2_DETUNE       = 44;
    static constexpr uint8_t OSC1_FINE_TUNE    = 45; // Fine tune (–100..+100 cents)
    static constexpr uint8_t OSC2_FINE_TUNE    = 46;

    // --- Arbitrary waveform table selection (per oscillator) ---
    // When the oscillator waveform is ARBITRARY, these CCs select which
    // AKWF piano table to load.  Values map evenly across the available
    // tables via arbIndexFromCC().
    static constexpr uint8_t OSC1_ARB_INDEX   = 83;
    static constexpr uint8_t OSC2_ARB_INDEX   = 85;

    // -------------------------------------------------------------------------
    // Oscillator mix/balance and supersaw parameters
    // -------------------------------------------------------------------------
    static constexpr uint8_t OSC_MIX_BALANCE   = 47; // Cross‑fader between OSC1/OSC2
    static constexpr uint8_t OSC1_MIX          = 60; // Dedicated OSC1 mixer tap
    static constexpr uint8_t OSC2_MIX          = 61; // Dedicated OSC2 mixer tap
    static constexpr uint8_t SUPERSAW1_DETUNE  = 77; // Osc1 supersaw detune amount
    static constexpr uint8_t SUPERSAW1_MIX     = 78; // Osc1 supersaw mix amount
    static constexpr uint8_t SUPERSAW2_DETUNE  = 79; // Osc2 supersaw detune amount
    static constexpr uint8_t SUPERSAW2_MIX     = 80; // Osc2 supersaw mix amount
    static constexpr uint8_t SUB_MIX           = 58; // Sub oscillator level
    static constexpr uint8_t NOISE_MIX         = 59; // Noise mix level

    // -------------------------------------------------------------------------
    // Filter modulation and key tracking
    // -------------------------------------------------------------------------
    static constexpr uint8_t FILTER_ENV_AMOUNT  = 48; // Envelope amount (–1..1)
    static constexpr uint8_t FILTER_KEY_TRACK   = 50; // Key tracking amount (–1..1)
    static constexpr uint8_t FILTER_OCTAVE_CONTROL = 84; // Filter octave control (0..10)

    // -------------------------------------------------------------------------
    // Low frequency oscillators (LFO)
    // -------------------------------------------------------------------------
    // LFO2 (Page 12)
    static constexpr uint8_t LFO2_FREQ        = 51;
    static constexpr uint8_t LFO2_DEPTH       = 52;
    static constexpr uint8_t LFO2_DESTINATION = 53;

    // LFO1 (Page 11)
    static constexpr uint8_t LFO1_FREQ        = 54;
    static constexpr uint8_t LFO1_DEPTH       = 55;
    static constexpr uint8_t LFO1_DESTINATION = 56;

    // LFO waveforms
    static constexpr uint8_t LFO1_WAVEFORM    = 62;
    static constexpr uint8_t LFO2_WAVEFORM    = 63;

    // -------------------------------------------------------------------------
    // Effects
    // -------------------------------------------------------------------------
    static constexpr uint8_t FX_REVERB_SIZE     = 70;
    static constexpr uint8_t FX_REVERB_DAMP     = 71;
    static constexpr uint8_t FX_DELAY_TIME      = 72; // Base delay time (ms)
    static constexpr uint8_t FX_DELAY_FEEDBACK  = 73;
    static constexpr uint8_t FX_DRY_MIX         = 74;
    static constexpr uint8_t FX_REVERB_MIX      = 75;
    static constexpr uint8_t FX_DELAY_MIX       = 76;

    // -------------------------------------------------------------------------
    // Additional effect parameters for the new stereo ping‑pong delay and plate
    // reverb from hexefx_audiolib_i16.  These CCs are assigned starting at
    // 93 to avoid clashing with existing mappings.  See FXChainBlock for
    // implementation details.  When a mix parameter is zero the corresponding
    // effect will be bypassed to save CPU cycles.
    // -------------------------------------------------------------------------

    // Plate reverb damping is now split into separate high‑ and low‑frequency
    // damping controls.  FX_REVERB_DAMP controls the high band (0..1).  The
    // low band uses this new CC.  A value of 0.0 yields no damping, 1.0
    // shortens the low frequency decay substantially.
    static constexpr uint8_t FX_REVERB_LODAMP    = 93;

    // Stereo ping‑pong delay modulation rate (0..1).  Higher values increase
    // the speed of the internal LFO that modulates the delay time, producing
    // chorusing and flanging effects.  Use in conjunction with mod depth.
    static constexpr uint8_t FX_DELAY_MOD_RATE   = 94;

    // Stereo ping‑pong delay modulation depth (0..1).  Controls the amount
    // of delay time modulation.  A value of 0 disables modulation; 1.0
    // produces strong pitch‑bending effects.
    static constexpr uint8_t FX_DELAY_MOD_DEPTH  = 95;

    // Stereo ping‑pong delay inertia (0..1).  This controls the inertia of
    // the internal diffusion network; higher values smooth transitions and
    // smear echoes.  Lower values produce crisp repeats.
    static constexpr uint8_t FX_DELAY_INERTIA    = 96;

    // High‑frequency tone control for the ping‑pong delay (0..1).  Lower
    // values darken the repeats by rolling off treble; higher values keep
    // more high end.  Internally this maps to the treble parameter of
    // AudioEffectDelayStereo_i16.
    static constexpr uint8_t FX_DELAY_TREBLE     = 97;

    // Low‑frequency tone control for the ping‑pong delay (0..1).  Lower
    // values reduce bass in the repeats; higher values preserve or boost
    // low frequencies.  Internally this maps to the bass parameter of
    // AudioEffectDelayStereo_i16.
    static constexpr uint8_t FX_DELAY_BASS       = 98;

    // -------------------------------------------------------------------------
    // Glide and global modulation
    // -------------------------------------------------------------------------
    static constexpr uint8_t GLIDE_ENABLE      = 81;
    static constexpr uint8_t GLIDE_TIME        = 82; // Glide time (ms)
    static constexpr uint8_t AMP_MOD_FIXED_LEVEL = 90; // Amp modulator DC level

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
    // Arbitrary waveform bank selection
    // -------------------------------------------------------------------------
    // These CCs select which AKWF bank to use for arbitrary waveforms on each
    // oscillator.  Values are evenly binned across the number of banks defined
    // in AKWF_All.h.  See SynthEngine::handleControlChange for mapping logic.
    static constexpr uint8_t OSC1_ARB_BANK = 101;
    static constexpr uint8_t OSC2_ARB_BANK = 102;



    // -------------------------------------------------------------------------
    // Utility: return a human‑readable name for a CC.  If a CC is not
    // enumerated above, nullptr is returned.  You can use this for display
    // purposes or debugging.
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
            case FX_REVERB_SIZE:      return "Rev Size";
            case FX_REVERB_DAMP:      return "Rev Damp";
            case FX_DELAY_TIME:       return "Delay Time";
            case FX_DELAY_FEEDBACK:   return "Delay FB";
            case FX_DRY_MIX:          return "Dry Mix";
            case FX_REVERB_MIX:       return "Rev Mix";
            case FX_DELAY_MIX:        return "Delay Mix";
            // New effect parameters
            case FX_REVERB_LODAMP:    return "Rev LoDamp";
            case FX_DELAY_MOD_RATE:   return "Dly ModRate";
            case FX_DELAY_MOD_DEPTH:  return "Dly ModDepth";
            case FX_DELAY_INERTIA:    return "Dly Inertia";
            case FX_DELAY_TREBLE:     return "Dly Treble";
            case FX_DELAY_BASS:       return "Dly Bass";
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
