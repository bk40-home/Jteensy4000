/*
 * AudioEffectJPFX.h (STEREO OUTPUT VERSION)
 *
 * This module implements the effects section of the Roland JP-8000 synthesizer
 * for the JT-4000 project.
 *
 * KEY CHANGES FOR STEREO OUTPUT:
 * 1. Changed from 1 output to 2 outputs (true stereo!)
 * 2. Maintains 1 input (mono input, stereo processing, stereo output)
 * 3. Separate transmit() calls for left and right channels
 * 4. Preserves stereo imaging from ping-pong delays and modulation
 *
 * PREVIOUS BUG FIXES MAINTAINED:
 * - 1 input (mono processing, internal stereo)
 * - Separate modulation and delay buffers (no conflicts)
 * - Continuous processing (maintains effect tails)
 * - Smart bypass when effects disabled
 */

#pragma once

#include <Arduino.h>
#include "AudioStream.h"

// Maximum delay time in milliseconds.  The JP-8000's delay extends up to
// 1250ms; we allocate a little extra headroom.  Changing this constant
// will directly affect the size of the delay buffer allocated at
// construction time.  If PSRAM is available it will be used.
#define JPFX_MAX_DELAY_MS    1500.0f

// Number of modulation effect variations (chorus/flanger/phaser)
#define JPFX_NUM_MOD_VARIATIONS 11

// Number of delay effect variations
#define JPFX_NUM_DELAY_VARIATIONS 5

class AudioEffectJPFX : public AudioStream {
public:
    // Enumeration for selecting the modulation effect variation
    enum ModEffectType {
        JPFX_MOD_OFF = -1,
        JPFX_CHORUS1 = 0,
        JPFX_CHORUS2,
        JPFX_CHORUS3,
        JPFX_FLANGER1,
        JPFX_FLANGER2,
        JPFX_FLANGER3,
        JPFX_PHASER1,
        JPFX_PHASER2,
        JPFX_PHASER3,
        JPFX_PHASER4,
        JPFX_CHORUS_DEEP
    };

    // Enumeration for selecting the delay effect variation
    enum DelayEffectType {
        JPFX_DELAY_OFF = -1,
        JPFX_DELAY_SHORT = 0,
        JPFX_DELAY_LONG,
        JPFX_DELAY_PINGPONG1,
        JPFX_DELAY_PINGPONG2,
        JPFX_DELAY_PINGPONG3
    };

    // Constructor: 1 input (mono), 2 outputs (stereo)
    AudioEffectJPFX();
    
    // Destructor to free delay buffers
    ~AudioEffectJPFX();

    // Override update() from AudioStream
    virtual void update(void) override;

    // ----- Tone control interface -----
    void setBassGain(float dB);    // ±12 dB range, default 0
    void setTrebleGain(float dB);  // ±12 dB range, default 0

    // ----- Modulation effect interface -----
    void setModEffect(ModEffectType type);
    void setModMix(float mix);          // 0.0..1.0, default 0.5
    void setModRate(float rate);        // Hz, 0 = use preset
    void setModFeedback(float fb);      // 0.0..0.99, -1 = use preset

    // ----- Delay effect interface -----
    void setDelayEffect(DelayEffectType type);
    void setDelayMix(float mix);        // 0.0..1.0, default 0.5
    void setDelayFeedback(float fb);    // 0.0..0.99, -1 = use preset
    void setDelayTime(float ms);        // ms, 0 = use preset

private:
    // Input queue for AudioStream (1 input)
    audio_block_t *inputQueueArray[1];

    // ----- Tone control internals -----
    typedef struct {
        float b0, b1, a1;  // Filter coefficients
        float in1, out1;   // Filter state
    } ShelfFilter;

    ShelfFilter bassFilterL, bassFilterR;
    ShelfFilter trebleFilterL, trebleFilterR;
    float targetBassGain, targetTrebleGain;
    bool toneDirty;

    void computeShelfCoeffs(ShelfFilter &filt, float cornerHz, float gainDB, bool high);
    inline void applyTone(float &l, float &r);

    // ----- Modulation effect internals -----
    typedef struct {
        float baseDelayL, baseDelayR;   // Base delay (ms)
        float depthL, depthR;           // Modulation depth (ms)
        float rate;                     // LFO rate (Hz)
        float feedback;                 // Feedback (0.0..0.99)
        float mix;                      // Wet/dry mix (0.0..1.0)
        bool  isPhaser;                 // Use all-pass instead of delay
        bool  isFlanger;                // Use shorter delay times
    } ModParams;

    static const ModParams modParams[JPFX_NUM_MOD_VARIATIONS];

    ModEffectType modType;
    float modMix;
    float modRateOverride;
    float modFeedbackOverride;
    float lfoPhaseL, lfoPhaseR;
    float lfoIncL, lfoIncR;

    void updateLfoIncrements();
    inline void processModulation(float inL, float inR, float &outL, float &outR);

    // ----- Delay effect internals -----
    typedef struct {
        float delayL, delayR;  // Delay time (ms)
        float feedback;         // Feedback (0.0..0.99)
        float mix;              // Wet/dry mix (0.0..1.0)
    } DelayParams;

    static const DelayParams delayParams[JPFX_NUM_DELAY_VARIATIONS];

    DelayEffectType delayType;
    float delayMix;
    float delayFeedbackOverride;
    float delayTimeOverride;

    // Separate delay buffers for modulation and delay effects
    float *modBufL, *modBufR;         // Modulation delay buffers
    float *delayBufL, *delayBufR;     // Delay effect buffers
    uint32_t modBufSize, delayBufSize;
    uint32_t modWriteIndex, delayWriteIndex;

    void allocateDelayBuffers();
    inline void processDelay(float inL, float inR, float &outL, float &outR);
};