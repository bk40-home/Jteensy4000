#pragma once
// -----------------------------------------------------------------------------
// AudioFilterOBXa
// -----------------------------------------------------------------------------
// Teensy AudioStream wrapper around an OB-Xf style filter core (2-pole + 4-pole).
//
// Features:
//  - Selectable 2-pole or 4-pole processing (setTwoPole()).
//  - Optional Xpander 4-pole pole-mix modes (setXpander4Pole / setXpanderMode).
//  - Optional 2-pole behaviours (BP blend / push) for parity with original core.
//  - Modulation inputs (Audio.h-style): audio in + cutoffMod + resonanceMod.
//  - Control-rate modulation: key tracking + envelope amount (optional).
//  - Debug capture with **pre-event** ring + **rising-edge** fault latch
//    to avoid log spam; safe recovery/reset when unstable.
//
// Wiring (3 inputs):
//   input 0: audio
//   input 1: cutoff modulation bus  (-1..+1), scaled by setCutoffModOctaves()
//   input 2: resonance modulation bus (-1..+1), scaled by setResonanceModDepth()
//
// Debug usage:
//   - Ensure OBXA_DEBUG == 1
//   - Call obxa.debugFlush(Serial); from loop().
// -----------------------------------------------------------------------------

#include <Arduino.h>
#include <math.h>
#include "AudioStream.h"

#ifndef OBXA_DEBUG
#define OBXA_DEBUG 1
#endif

// When enabled, auto-resets internal poles when NaN/Inf or runaway values occur.
#ifndef OBXA_STATE_GUARD
#define OBXA_STATE_GUARD 1
#endif

// Max |pole| / |y0| before we consider it runaway.
#ifndef OBXA_HUGE_THRESHOLD
#define OBXA_HUGE_THRESHOLD 1.0e6f
#endif

// -----------------------------------------------------------------------------
// AudioFilterOBXa
// -----------------------------------------------------------------------------
class AudioFilterOBXa : public AudioStream
{
public:
    AudioFilterOBXa();

    // --- Core controls (match Teensy style) ---
    void frequency(float hz);
    void resonance(float r01);        // 0..1
    void multimode(float m01);        // 0..1 (when xpander4Pole=false)

    // --- Runtime mode toggles ---
    void setTwoPole(bool enabled);
    bool getTwoPole() const { return _useTwoPole; }

    void setXpander4Pole(bool enabled);
    bool getXpander4Pole() const { return _xpander4Pole; }

    void setXpanderMode(uint8_t mode);   // 0..14
    uint8_t getXpanderMode() const { return _xpanderMode; }

    // 2-pole options (only used in 2-pole mode)
    void setBPBlend2Pole(bool enabled);
    bool getBPBlend2Pole() const { return _bpBlend2Pole; }

    void setPush2Pole(bool enabled);
    bool getPush2Pole() const { return _push2Pole; }

    // --- Modulation scaling (Audio input busses) ---
    // Cutoff modulation amount in octaves per +1.0 on input1
    void setCutoffModOctaves(float oct);
    float getCutoffModOctaves() const { return _cutoffModOct; }

    // Resonance modulation depth in resonance-01 units per +1.0 on input2
    void setResonanceModDepth(float depth01);
    float getResonanceModDepth() const { return _resModDepth; }

    // --- Control-rate modulation (optional) ---
    // Key tracking: octaves per octave (0..1 typical). Uses current midiNote.
    void setKeyTrack(float amount01);
    float getKeyTrack() const { return _keyTrack; }

    // Envelope modulation: octaves per +1 envelope value (0..1 env typical).
    void setEnvModOctaves(float oct);
    float getEnvModOctaves() const { return _envModOct; }

    void setMidiNote(float note);    // 0..127
    float getMidiNote() const { return _midiNote; }

    void setEnvValue(float env01);   // 0..1 (latest envelope sample)
    float getEnvValue() const { return _envValue; }

#if OBXA_DEBUG
    // Flush captured debug events (fault + pre-events) to a Stream.
    void debugFlush(Stream &s);

    // Optional: clear latched fault without waiting for stable samples.
    void debugClearFault();
#endif

    virtual void update(void) override;

private:
    audio_block_t *_inQ[3]{};

    // Internal control state
    float _cutoffHzTarget = 1000.0f;
    float _res01Target    = 0.0f;
    float _multimode01    = 0.0f;

    bool    _useTwoPole   = false;
    bool    _xpander4Pole = false;
    uint8_t _xpanderMode  = 0;
    bool    _bpBlend2Pole = false;
    bool    _push2Pole    = false;

    float _cutoffModOct = 0.0f;
    float _resModDepth  = 0.0f;

    float _keyTrack  = 0.0f;
    float _envModOct = 0.0f;
    float _midiNote  = 60.0f;
    float _envValue  = 0.0f;

    // recovery / guard
    uint16_t _cooldownBlocks = 0;

    // Forward-declared core (defined in .cpp)
    struct Core;
    Core *_core = nullptr;

#if OBXA_DEBUG
    // Wrapper-side debug event ring (small, ISR-safe)
    struct DebugEvent
    {
        uint32_t ms = 0;
        bool twoPole = false;

        float fs = 0.0f;
        float cutoffHz = 0.0f;
        float resonance01 = 0.0f;

        float x = 0.0f;
        float out = 0.0f;

        float g = 0.0f;
        float lpc = 0.0f;
        float res4Pole = 0.0f;
        float denom = 0.0f;
        float S = 0.0f;
        float G = 0.0f;

        float y0 = 0.0f, y1 = 0.0f, y2 = 0.0f, y3 = 0.0f, y4 = 0.0f;
        float pole1 = 0.0f, pole2 = 0.0f, pole3 = 0.0f, pole4 = 0.0f;

        uint8_t reason = 0; // 1=nonfinite, 2=huge, 3=denom-small
        uint8_t stage  = 0;
    };

    static constexpr uint8_t DBG_RING_SIZE = 32;
    DebugEvent _dbgRing[DBG_RING_SIZE]{};
    volatile uint8_t _dbgWrite = 0;
    volatile uint8_t _dbgRead  = 0;

    inline void dbgPush(const DebugEvent &e);

    // "Fault latch" so we only emit once per fault burst.
    volatile bool _faultLatched = false;

    // Core hook thunks
    static void coreHookThunk(void *ctx, const void *evt);
    void onCoreEvent(const void *evt);
#endif
};
