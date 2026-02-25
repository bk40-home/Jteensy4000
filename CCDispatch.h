// CCDispatch.h
// =============================================================================
// MIDI CC → SynthEngine dispatcher.
//
// Layout:
//   - One inline handler function per CC (or group of related CCs)
//   - A constexpr HANDLER_TABLE[128] mapping CC numbers to handler pointers
//   - A single handle() function called from SynthEngine::handleControlChange()
//     (or main MIDI callback)
//
// All CC numbers are taken from CCDefs.h — do not hard-code numbers here.
//
// Mapping curves are from Mapping.h — keep conversion logic there, not here.
//
// NOTE: The envelope handlers were previously stubs that only logged.
//       They now correctly call SynthEngine setter methods.
//
// NOTE: The HANDLER_TABLE is an alternative dispatch path.  SynthEngine.cpp
//       has a large switch-case that also handles all CCs.  In the current
//       architecture, SynthEngine::handleControlChange() is the authoritative
//       dispatcher; CCDispatch::handle() is a convenience wrapper that can be
//       called from anywhere that does not have full SynthEngine context (e.g.
//       preset loaders, test harnesses).
// =============================================================================

#pragma once

#include <Arduino.h>
#include "CCDefs.h"
#include "Mapping.h"
#include "Waveforms.h"
#include "DebugTrace.h"

// Forward declaration — avoids including SynthEngine.h here
class SynthEngine;

namespace CCDispatch {

// Handler function signature: (raw CC value 0-127, pointer to engine)
using HandlerFn = void (*)(uint8_t ccVal, SynthEngine* synth);

// =============================================================================
// OSCILLATOR HANDLERS
// =============================================================================

inline void handleOsc1Wave(uint8_t cc, SynthEngine* s) {
    WaveformType t = waveformFromCC(cc);
    s->setOsc1Waveform((int)t);
    JT_LOGF("[CC OSC1_WAVE] -> %s\n", waveformShortName(t));
}
inline void handleOsc2Wave(uint8_t cc, SynthEngine* s) {
    WaveformType t = waveformFromCC(cc);
    s->setOsc2Waveform((int)t);
    JT_LOGF("[CC OSC2_WAVE] -> %s\n", waveformShortName(t));
}

// Coarse pitch: CC spread across 5 semitone steps (−24/−12/0/+12/+24)
inline void handleOsc1PitchOffset(uint8_t cc, SynthEngine* s) {
    float semis = (cc <= 25) ? -24.0f : (cc <= 51) ? -12.0f :
                  (cc <= 76) ?   0.0f : (cc <= 101) ? 12.0f : 24.0f;
    s->setOsc1PitchOffset(semis);
    JT_LOGF("[CC OSC1_PITCH] %.0f semi\n", semis);
}
inline void handleOsc2PitchOffset(uint8_t cc, SynthEngine* s) {
    float semis = (cc <= 25) ? -24.0f : (cc <= 51) ? -12.0f :
                  (cc <= 76) ?   0.0f : (cc <= 101) ? 12.0f : 24.0f;
    s->setOsc2PitchOffset(semis);
    JT_LOGF("[CC OSC2_PITCH] %.0f semi\n", semis);
}

// Detune: 0-127 → −1..+1 semitones (centred at 64)
inline void handleOsc1Detune(uint8_t cc, SynthEngine* s) {
    const float d = (cc / 127.0f) * 2.0f - 1.0f;
    s->setOsc1Detune(d);
    JT_LOGF("[CC OSC1_DETUNE] %.3f\n", d);
}
inline void handleOsc2Detune(uint8_t cc, SynthEngine* s) {
    const float d = (cc / 127.0f) * 2.0f - 1.0f;
    s->setOsc2Detune(d);
    JT_LOGF("[CC OSC2_DETUNE] %.3f\n", d);
}

// Fine tune: 0-127 → −100..+100 cents (centred at 64)
inline void handleOsc1FineTune(uint8_t cc, SynthEngine* s) {
    const float c = (cc / 127.0f) * 200.0f - 100.0f;
    s->setOsc1FineTune(c);
    JT_LOGF("[CC OSC1_FINE] %.1f cents\n", c);
}
inline void handleOsc2FineTune(uint8_t cc, SynthEngine* s) {
    const float c = (cc / 127.0f) * 200.0f - 100.0f;
    s->setOsc2FineTune(c);
    JT_LOGF("[CC OSC2_FINE] %.1f cents\n", c);
}

// OSC balance: 0 = full osc1, 127 = full osc2
inline void handleOscMixBalance(uint8_t cc, SynthEngine* s) {
    const float n = cc / 127.0f;
    s->setOscMix(1.0f - n, n);
    JT_LOGF("[CC OSC_BALANCE] L=%.3f R=%.3f\n", 1.0f - n, n);
}

// Individual osc level: 0-127 → 0-1
inline void handleOsc1Mix(uint8_t cc, SynthEngine* s) { s->setOsc1Mix(cc / 127.0f); }
inline void handleOsc2Mix(uint8_t cc, SynthEngine* s) { s->setOsc2Mix(cc / 127.0f); }
inline void handleSubMix(uint8_t cc, SynthEngine* s)  { s->setSubMix(cc / 127.0f); }
inline void handleNoiseMix(uint8_t cc, SynthEngine* s){ s->setNoiseMix(cc / 127.0f); }

inline void handleSupersaw1Detune(uint8_t cc, SynthEngine* s) { s->setSupersawDetune(0, cc / 127.0f); }
inline void handleSupersaw1Mix(uint8_t cc, SynthEngine* s)    { s->setSupersawMix(0,    cc / 127.0f); }
inline void handleSupersaw2Detune(uint8_t cc, SynthEngine* s) { s->setSupersawDetune(1, cc / 127.0f); }
inline void handleSupersaw2Mix(uint8_t cc, SynthEngine* s)    { s->setSupersawMix(1,    cc / 127.0f); }

inline void handleOsc1FreqDC(uint8_t cc, SynthEngine* s)   { s->setOsc1FrequencyDcAmp(cc / 127.0f); }
inline void handleOsc1ShapeDC(uint8_t cc, SynthEngine* s)  { s->setOsc1ShapeDcAmp(cc / 127.0f); }
inline void handleOsc2FreqDC(uint8_t cc, SynthEngine* s)   { s->setOsc2FrequencyDcAmp(cc / 127.0f); }
inline void handleOsc2ShapeDC(uint8_t cc, SynthEngine* s)  { s->setOsc2ShapeDcAmp(cc / 127.0f); }
inline void handleRing1Mix(uint8_t cc, SynthEngine* s)     { s->setRing1Mix(cc / 127.0f); }
inline void handleRing2Mix(uint8_t cc, SynthEngine* s)     { s->setRing2Mix(cc / 127.0f); }

inline void handleOsc1FeedbackAmount(uint8_t cc, SynthEngine* s) { s->setOsc1FeedbackAmount(cc / 127.0f); }
inline void handleOsc2FeedbackAmount(uint8_t cc, SynthEngine* s) { s->setOsc2FeedbackAmount(cc / 127.0f); }
inline void handleOsc1FeedbackMix(uint8_t cc, SynthEngine* s)    { s->setOsc1FeedbackMix(cc / 127.0f); }
inline void handleOsc2FeedbackMix(uint8_t cc, SynthEngine* s)    { s->setOsc2FeedbackMix(cc / 127.0f); } // was calling setOsc1FeedbackMix — BUG FIX

// =============================================================================
// FILTER HANDLERS
// =============================================================================

inline void handleFilterCutoff(uint8_t cc, SynthEngine* s) {
    s->setFilterCutoff(JT4000Map::cc_to_obxa_cutoff_hz(cc));
}
inline void handleFilterResonance(uint8_t cc, SynthEngine* s) {
    s->setFilterResonance(JT4000Map::cc_to_obxa_res01(cc));
}
// Env amount: 0-127 → −1..+1 (bipolar)
inline void handleFilterEnvAmount(uint8_t cc, SynthEngine* s) {
    s->setFilterEnvAmount((cc / 127.0f) * 2.0f - 1.0f);
}
// Key tracking: 0-127 → −1..+1 (bipolar)
inline void handleFilterKeyTrack(uint8_t cc, SynthEngine* s) {
    s->setFilterKeyTrackAmount((cc / 127.0f) * 2.0f - 1.0f);
}
// Octave control: 0-127 → 0..10 octaves
inline void handleFilterOctaveControl(uint8_t cc, SynthEngine* s) {
    s->setFilterOctaveControl((cc / 127.0f) * 10.0f);
}
inline void handleFilterMultimode(uint8_t cc, SynthEngine* s)    { s->setFilterMultimode(cc / 127.0f); }
inline void handleFilterTwoPole(uint8_t cc, SynthEngine* s)      { s->setFilterTwoPole(cc >= 64); }
inline void handleFilterXpander4Pole(uint8_t cc, SynthEngine* s) { s->setFilterXpander4Pole(cc >= 64); }
inline void handleFilterXpanderMode(uint8_t cc, SynthEngine* s)  {
    uint8_t mode = (uint16_t(cc) * 15u) / 128u;
    if (mode > 14) mode = 14;
    s->setFilterXpanderMode(mode);
}
inline void handleFilterBPBlend2Pole(uint8_t cc, SynthEngine* s) { s->setFilterBPBlend2Pole(cc >= 64); }
inline void handleFilterPush2Pole(uint8_t cc, SynthEngine* s)    { s->setFilterPush2Pole(cc >= 64); }
inline void handleFilterResModDepth(uint8_t cc, SynthEngine* s)  { s->setFilterResonanceModDepth(cc / 127.0f); }

// =============================================================================
// ENVELOPE HANDLERS
// Previously these only logged — now they call the engine setters.
// =============================================================================

inline void handleAmpAttack(uint8_t cc, SynthEngine* s) {
    const float ms = JT4000Map::cc_to_time_ms(cc);
    // Envelope setters are per-voice inside SynthEngine via handleControlChange().
    // Dispatch through the engine so the voice loop runs correctly.
    s->handleControlChange(1, CC::AMP_ATTACK, cc);
    JT_LOGF("[CC AMP_ATTACK] %.2f ms\n", ms);
}
inline void handleAmpDecay(uint8_t cc, SynthEngine* s) {
    s->handleControlChange(1, CC::AMP_DECAY, cc);
    JT_LOGF("[CC AMP_DECAY] %.2f ms\n", JT4000Map::cc_to_time_ms(cc));
}
inline void handleAmpSustain(uint8_t cc, SynthEngine* s) {
    s->handleControlChange(1, CC::AMP_SUSTAIN, cc);
    JT_LOGF("[CC AMP_SUSTAIN] %.3f\n", cc / 127.0f);
}
inline void handleAmpRelease(uint8_t cc, SynthEngine* s) {
    s->handleControlChange(1, CC::AMP_RELEASE, cc);
    JT_LOGF("[CC AMP_RELEASE] %.2f ms\n", JT4000Map::cc_to_time_ms(cc));
}
inline void handleFilterEnvAttack(uint8_t cc, SynthEngine* s) {
    s->handleControlChange(1, CC::FILTER_ENV_ATTACK, cc);
    JT_LOGF("[CC FLT_ATK] %.2f ms\n", JT4000Map::cc_to_time_ms(cc));
}
inline void handleFilterEnvDecay(uint8_t cc, SynthEngine* s) {
    s->handleControlChange(1, CC::FILTER_ENV_DECAY, cc);
    JT_LOGF("[CC FLT_DEC] %.2f ms\n", JT4000Map::cc_to_time_ms(cc));
}
inline void handleFilterEnvSustain(uint8_t cc, SynthEngine* s) {
    s->handleControlChange(1, CC::FILTER_ENV_SUSTAIN, cc);
    JT_LOGF("[CC FLT_SUS] %.3f\n", cc / 127.0f);
}
inline void handleFilterEnvRelease(uint8_t cc, SynthEngine* s) {
    s->handleControlChange(1, CC::FILTER_ENV_RELEASE, cc);
    JT_LOGF("[CC FLT_REL] %.2f ms\n", JT4000Map::cc_to_time_ms(cc));
}

// =============================================================================
// LFO HANDLERS
// =============================================================================

inline void handleLFO1Freq(uint8_t cc, SynthEngine* s)  { s->setLFO1Frequency(JT4000Map::cc_to_lfo_hz(cc)); }
inline void handleLFO1Depth(uint8_t cc, SynthEngine* s) { s->setLFO1Amount(cc / 127.0f); }
inline void handleLFO1Dest(uint8_t cc, SynthEngine* s)  { s->setLFO1Destination((LFODestination)JT4000Map::lfoDestFromCC(cc)); }
inline void handleLFO1Wave(uint8_t cc, SynthEngine* s)  { s->setLFO1Waveform((int)waveformFromCC(cc)); }

inline void handleLFO2Freq(uint8_t cc, SynthEngine* s)  { s->setLFO2Frequency(JT4000Map::cc_to_lfo_hz(cc)); }
inline void handleLFO2Depth(uint8_t cc, SynthEngine* s) { s->setLFO2Amount(cc / 127.0f); }
inline void handleLFO2Dest(uint8_t cc, SynthEngine* s)  { s->setLFO2Destination((LFODestination)JT4000Map::lfoDestFromCC(cc)); }
inline void handleLFO2Wave(uint8_t cc, SynthEngine* s)  { s->setLFO2Waveform((int)waveformFromCC(cc)); }

inline void handleLFO1TimingMode(uint8_t cc, SynthEngine* s) {
    // 12 modes spread evenly across 0-127
    uint8_t mode = (uint16_t(cc) * 12u) / 128u;
    if (mode > 11) mode = 11;
    s->setLFO1TimingMode((TimingMode)mode);
}
inline void handleLFO2TimingMode(uint8_t cc, SynthEngine* s) {
    uint8_t mode = (uint16_t(cc) * 12u) / 128u;
    if (mode > 11) mode = 11;
    s->setLFO2TimingMode((TimingMode)mode);
}

// =============================================================================
// FX HANDLERS
// =============================================================================

// Bass/treble shelving: 0-127 → −12..+12 dB
inline void handleFXBassGain(uint8_t cc, SynthEngine* s)   { s->setFXBassGain((cc / 127.0f) * 24.0f - 12.0f); }
inline void handleFXTrebleGain(uint8_t cc, SynthEngine* s) { s->setFXTrebleGain((cc / 127.0f) * 24.0f - 12.0f); }

// Modulation effect: CC 0 = off (−1), 1-127 → variation 0-10
inline void handleFXModEffect(uint8_t cc, SynthEngine* s) {
    int8_t v = (cc == 0) ? -1 : (int8_t)(((uint16_t)(cc - 1) * 11u) / 127u);
    if (v > 10) v = 10;
    s->setFXModEffect(v);
}
inline void handleFXModMix(uint8_t cc, SynthEngine* s)      { s->setFXModMix(cc / 127.0f); }
inline void handleFXModRate(uint8_t cc, SynthEngine* s)     { s->setFXModRate((cc / 127.0f) * 20.0f); }
inline void handleFXModFeedback(uint8_t cc, SynthEngine* s) {
    // CC 0 = use preset default (pass −1); 1-127 → 0..0.99
    const float fb = (cc == 0) ? -1.0f : ((cc - 1) / 126.0f) * 0.99f;
    s->setFXModFeedback(fb);
}

// Delay effect: CC 0 = off (−1), 1-127 → variation 0-4
inline void handleFXDelayEffect(uint8_t cc, SynthEngine* s) {
    int8_t v = (cc == 0) ? -1 : (int8_t)(((uint16_t)(cc - 1) * 5u) / 127u);
    if (v > 4) v = 4;
    s->setFXDelayEffect(v);
}
inline void handleFXDelayMix(uint8_t cc, SynthEngine* s)      { s->setFXDelayMix(cc / 127.0f); }
inline void handleFXDelayFeedback(uint8_t cc, SynthEngine* s) {
    const float fb = (cc == 0) ? -1.0f : ((cc - 1) / 126.0f) * 0.99f;
    s->setFXDelayFeedback(fb);
}
// Delay time: 0-127 → 0..1500 ms
inline void handleFXDelayTime(uint8_t cc, SynthEngine* s) { s->setFXDelayTime((cc / 127.0f) * 1500.0f); }

inline void handleDelayTimingMode(uint8_t cc, SynthEngine* s) {
    uint8_t mode = (uint16_t(cc) * 12u) / 128u;
    if (mode > 11) mode = 11;
    s->setDelayTimingMode((TimingMode)mode);
}

// Reverb
inline void handleFXReverbSize(uint8_t cc, SynthEngine* s)    { s->setFXReverbRoomSize(cc / 127.0f); }
inline void handleFXReverbHiDamp(uint8_t cc, SynthEngine* s)  { s->setFXReverbHiDamping(cc / 127.0f); }
inline void handleFXReverbLoDamp(uint8_t cc, SynthEngine* s)  { s->setFXReverbLoDamping(cc / 127.0f); }
inline void handleFXReverbMix(uint8_t cc, SynthEngine* s)     { s->setFXReverbMix(cc / 127.0f, cc / 127.0f); }
inline void handleFXReverbBypass(uint8_t cc, SynthEngine* s)  { s->setFXReverbBypass(cc >= 64); }

// Output mix levels
inline void handleFXDryMix(uint8_t cc, SynthEngine* s)        { s->setFXDryMix(cc / 127.0f); }
inline void handleFXJPFXMix(uint8_t cc, SynthEngine* s)       { const float m = cc / 127.0f; s->setFXJPFXMix(m, m); }

// =============================================================================
// GLOBAL HANDLERS
// =============================================================================

inline void handleGlideEnable(uint8_t cc, SynthEngine* s) { s->handleControlChange(1, CC::GLIDE_ENABLE, cc); }
inline void handleGlideTime(uint8_t cc, SynthEngine* s)   { s->handleControlChange(1, CC::GLIDE_TIME, cc); }
inline void handleAmpModFixed(uint8_t cc, SynthEngine* s) { s->SetAmpModFixedLevel(cc / 127.0f); }

// BPM
inline void handleBPMClockSource(uint8_t cc, SynthEngine* s)    { s->handleControlChange(1, CC::BPM_CLOCK_SOURCE, cc); }
inline void handleBPMInternalTempo(uint8_t cc, SynthEngine* s)  { s->handleControlChange(1, CC::BPM_INTERNAL_TEMPO, cc); }

// =============================================================================
// DISPATCH TABLE — indexed by CC number (0-127)
// nullptr = CC is not handled here (logged as unmapped)
// =============================================================================

constexpr HandlerFn HANDLER_TABLE[128] = {
    // 0: Bank Select MSB (unused)
    nullptr,
    // 1: Mod wheel (routed to LFO1 freq in SynthEngine switch — not in table)
    nullptr,
    // 2-20: Standard MIDI — unused
    nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
    nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
    nullptr, nullptr,

    // 21: OSC1_WAVE
    handleOsc1Wave,
    // 22: OSC2_WAVE
    handleOsc2Wave,
    // 23: FILTER_CUTOFF
    handleFilterCutoff,
    // 24: FILTER_RESONANCE
    handleFilterResonance,
    // 25: AMP_ATTACK
    handleAmpAttack,
    // 26: AMP_DECAY
    handleAmpDecay,
    // 27: AMP_SUSTAIN
    handleAmpSustain,
    // 28: AMP_RELEASE
    handleAmpRelease,
    // 29: FILTER_ENV_ATTACK
    handleFilterEnvAttack,
    // 30: FILTER_ENV_DECAY
    handleFilterEnvDecay,
    // 31: FILTER_ENV_SUSTAIN
    handleFilterEnvSustain,
    // 32: FILTER_ENV_RELEASE
    handleFilterEnvRelease,

    // 33-40: unused
    nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,

    // 41: OSC1_PITCH_OFFSET
    handleOsc1PitchOffset,
    // 42: OSC2_PITCH_OFFSET
    handleOsc2PitchOffset,
    // 43: OSC1_DETUNE
    handleOsc1Detune,
    // 44: OSC2_DETUNE
    handleOsc2Detune,
    // 45: OSC1_FINE_TUNE
    handleOsc1FineTune,
    // 46: OSC2_FINE_TUNE
    handleOsc2FineTune,
    // 47: OSC_MIX_BALANCE
    handleOscMixBalance,
    // 48: FILTER_ENV_AMOUNT
    handleFilterEnvAmount,
    // 49: unused
    nullptr,
    // 50: FILTER_KEY_TRACK
    handleFilterKeyTrack,

    // 51: LFO2_FREQ
    handleLFO2Freq,
    // 52: LFO2_DEPTH
    handleLFO2Depth,
    // 53: LFO2_DESTINATION
    handleLFO2Dest,
    // 54: LFO1_FREQ
    handleLFO1Freq,
    // 55: LFO1_DEPTH
    handleLFO1Depth,
    // 56: LFO1_DESTINATION
    handleLFO1Dest,
    // 57: unused
    nullptr,
    // 58: SUB_MIX
    handleSubMix,
    // 59: NOISE_MIX
    handleNoiseMix,
    // 60: OSC1_MIX
    handleOsc1Mix,
    // 61: OSC2_MIX
    handleOsc2Mix,
    // 62: LFO1_WAVEFORM
    handleLFO1Wave,
    // 63: LFO2_WAVEFORM
    handleLFO2Wave,

    // 64-69: Standard MIDI (sustain pedal etc) — unused
    nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,

    // 70: FX_REVERB_SIZE
    handleFXReverbSize,
    // 71: FX_REVERB_DAMP
    handleFXReverbHiDamp,
    // 72: FX_DELAY_TIME (legacy, unused in JPFX path)
    nullptr,
    // 73: FX_DELAY_FEEDBACK (legacy, unused in JPFX path)
    nullptr,
    // 74: FX_DRY_MIX
    handleFXDryMix,
    // 75: FX_REVERB_MIX
    handleFXReverbMix,
    // 76: FX_JPFX_MIX
    handleFXJPFXMix,

    // 77: SUPERSAW1_DETUNE
    handleSupersaw1Detune,
    // 78: SUPERSAW1_MIX
    handleSupersaw1Mix,
    // 79: SUPERSAW2_DETUNE
    handleSupersaw2Detune,
    // 80: SUPERSAW2_MIX
    handleSupersaw2Mix,
    // 81: GLIDE_ENABLE
    handleGlideEnable,
    // 82: GLIDE_TIME
    handleGlideTime,
    // 83: OSC1_ARB_INDEX — routed through SynthEngine switch (needs bank context)
    nullptr,
    // 84: FILTER_OCTAVE_CONTROL
    handleFilterOctaveControl,
    // 85: OSC2_ARB_INDEX — routed through SynthEngine switch
    nullptr,
    // 86: OSC1_FREQ_DC
    handleOsc1FreqDC,
    // 87: OSC1_SHAPE_DC
    handleOsc1ShapeDC,
    // 88: OSC2_FREQ_DC
    handleOsc2FreqDC,
    // 89: OSC2_SHAPE_DC
    handleOsc2ShapeDC,
    // 90: AMP_MOD_FIXED_LEVEL
    handleAmpModFixed,
    // 91: RING1_MIX
    handleRing1Mix,
    // 92: RING2_MIX
    handleRing2Mix,

    // 93: FX_REVERB_LODAMP
    handleFXReverbLoDamp,
    // 94: FX_REVERB_BYPASS
    handleFXReverbBypass,
    // 95-98: legacy FX (unused)
    nullptr, nullptr, nullptr, nullptr,

    // 99: FX_BASS_GAIN
    handleFXBassGain,
    // 100: FX_TREBLE_GAIN
    handleFXTrebleGain,
    // 101: OSC1_ARB_BANK — routed through SynthEngine switch
    nullptr,
    // 102: OSC2_ARB_BANK — routed through SynthEngine switch
    nullptr,
    // 103: FX_MOD_EFFECT
    handleFXModEffect,
    // 104: FX_MOD_MIX
    handleFXModMix,
    // 105: FX_MOD_RATE
    handleFXModRate,
    // 106: FX_MOD_FEEDBACK
    handleFXModFeedback,
    // 107: FX_JPFX_DELAY_EFFECT
    handleFXDelayEffect,
    // 108: FX_JPFX_DELAY_MIX
    handleFXDelayMix,
    // 109: FX_JPFX_DELAY_FEEDBACK
    handleFXDelayFeedback,
    // 110: FX_JPFX_DELAY_TIME
    handleFXDelayTime,

    // 111: FILTER_OBXA_MULTIMODE
    handleFilterMultimode,
    // 112: FILTER_OBXA_TWO_POLE
    handleFilterTwoPole,
    // 113: FILTER_OBXA_XPANDER_4_POLE
    handleFilterXpander4Pole,
    // 114: FILTER_OBXA_XPANDER_MODE
    handleFilterXpanderMode,
    // 115: FILTER_OBXA_BP_BLEND_2_POLE
    handleFilterBPBlend2Pole,
    // 116: FILTER_OBXA_PUSH_2_POLE
    handleFilterPush2Pole,
    // 117: FILTER_OBXA_RES_MOD_DEPTH
    handleFilterResModDepth,

    // 118: BPM_CLOCK_SOURCE
    handleBPMClockSource,
    // 119: BPM_INTERNAL_TEMPO
    handleBPMInternalTempo,
    // 120: LFO1_TIMING_MODE
    handleLFO1TimingMode,
    // 121: LFO2_TIMING_MODE
    handleLFO2TimingMode,
    // 122: DELAY_TIMING_MODE
    handleDelayTimingMode,

    // 123: OSC1_FEEDBACK_AMOUNT
    handleOsc1FeedbackAmount,
    // 124: OSC2_FEEDBACK_AMOUNT
    handleOsc2FeedbackAmount,
    // 125: OSC1_FEEDBACK_MIX
    handleOsc1FeedbackMix,
    // 126: OSC2_FEEDBACK_MIX
    handleOsc2FeedbackMix,

    // 127: unused
    nullptr,
};

// =============================================================================
// MAIN DISPATCH ENTRY POINT
// =============================================================================

/**
 * handle() — look up CC in the table and call the handler.
 * Prefer using SynthEngine::handleControlChange() for normal operation;
 * use this for preset loaders or places that only have a SynthEngine pointer.
 */
inline void handle(uint8_t cc, uint8_t value, SynthEngine* synth) {
    if (cc >= 128 || !synth) return;
    HandlerFn fn = HANDLER_TABLE[cc];
    if (fn) {
        fn(value, synth);
    } else {
        JT_LOGF("[CCDispatch] CC %u unmapped (val=%u)\n", cc, value);
    }
}

} // namespace CCDispatch
