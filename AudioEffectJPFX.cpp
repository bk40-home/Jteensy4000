/*
 * AudioEffectJPFX.cpp
 *
 * Implementation of the JP‑8000 FX section for the JT‑4000 project.  See
 * the header for a detailed description.  The code below follows the
 * structure laid out in AudioEffectJPFX.h and implements tone control,
 * modulation effects (chorus/flanger/phaser) and delay effects.  All
 * algorithms are intentionally lightweight to conserve CPU resources on
 * the Teensy platform.  Where possible, operations are consolidated and
 * computed once per block rather than once per sample.  Long delay
 * buffers are allocated dynamically; if PSRAM is available it will be
 * used.
 */

#include "AudioEffectJPFX.h"
// Include math.h for sinf(), powf() and tanf()
#include <math.h>
// Include arm_math.h for optional optimised functions; this header is
// available on the Teensy platform and provides SIMD implementations
// for sinf() and other math operations.  If not available the
// standard implementations will be used instead.
#ifdef __arm__
#include <arm_math.h>
#endif

// The AudioStream library defines AUDIO_SAMPLE_RATE_EXACT and
// AUDIO_BLOCK_SAMPLES.  We include Arduino.h via the header so these
// symbols are available.

//-----------------------------------------------------------------------------
// Modulation presets
//
// The JP‑8000 offers eleven modulation effect variations including
// choruses, flangers and phasers【536569142423667†L218-L224】.  Each preset is a small
// collection of parameters describing the base delay time, modulation
// depth and rate, feedback and mix for the left and right channels.  These
// values were derived by listening to the original hardware and by
// consulting various JP‑8000 resources.  Feel free to tweak them to
// taste.  Note that the flanger presets use shorter base delay times
// whereas phaser presets use slightly deeper modulation and a small
// amount of feedback to create notches in the spectrum.

const AudioEffectJPFX::ModParams AudioEffectJPFX::modParams[JPFX_NUM_MOD_VARIATIONS] = {
    /* JPFX_CHORUS1: gentle slow chorus */
    {15.0f, 15.0f,  2.0f,  4.0f, 0.25f, 0.0f, 0.5f, false, false},
    /* JPFX_CHORUS2: medium chorus */
    {20.0f, 20.0f,  3.0f,  5.0f, 0.80f, 0.0f, 0.6f, false, false},
    /* JPFX_CHORUS3: deep chorus */
    {25.0f, 25.0f,  4.0f,  6.0f, 0.40f, 0.0f, 0.7f, false, false},
    /* JPFX_FLANGER1: light flanger */
    { 3.0f,  3.0f,  2.0f,  2.0f, 0.50f, 0.5f, 0.5f, false, true },
    /* JPFX_FLANGER2: medium flanger */
    { 5.0f,  5.0f,  2.5f,  2.5f, 0.35f, 0.7f, 0.5f, false, true },
    /* JPFX_FLANGER3: fast flanger */
    { 2.0f,  2.0f,  1.0f,  1.0f, 1.50f, 0.3f, 0.4f, false, true },
    /* JPFX_PHASER1: gentle phaser */
    { 0.0f,  0.0f,  4.0f,  4.0f, 0.25f, 0.6f, 0.5f, true, false},
    /* JPFX_PHASER2: medium phaser */
    { 0.0f,  0.0f,  5.0f,  5.0f, 0.50f, 0.7f, 0.5f, true, false},
    /* JPFX_PHASER3: deep phaser */
    { 0.0f,  0.0f,  6.0f,  6.0f, 0.10f, 0.8f, 0.5f, true, false},
    /* JPFX_PHASER4: fast phaser */
    { 0.0f,  0.0f,  3.0f,  3.0f, 1.20f, 0.5f, 0.6f, true, false},
    /* JPFX_CHORUS_DEEP: very deep chorus with long delay */
    {30.0f, 30.0f, 10.0f, 12.0f, 0.20f, 0.0f, 0.7f, false, false}
};

//-----------------------------------------------------------------------------
// Delay presets
//
// The JP‑8000 delay section offers five variations【536569142423667†L218-L224】.  Each preset
// defines independent left and right delay times, feedback and mix.  The
// ping‑pong variations use different times for the left and right
// channels to create movement across the stereo field.

const AudioEffectJPFX::DelayParams AudioEffectJPFX::delayParams[JPFX_NUM_DELAY_VARIATIONS] = {
    /* JPFX_DELAY_SHORT: short mono delay */
    {150.0f, 150.0f, 0.30f, 0.5f},
    /* JPFX_DELAY_LONG: long mono delay */
    {500.0f, 500.0f, 0.40f, 0.5f},
    /* JPFX_DELAY_PINGPONG1: ping‑pong delay 1 */
    {300.0f, 600.0f, 0.40f, 0.6f},
    /* JPFX_DELAY_PINGPONG2: ping‑pong delay 2 */
    {150.0f, 300.0f, 0.50f, 0.6f},
    /* JPFX_DELAY_PINGPONG3: ping‑pong delay 3 */
    {400.0f, 200.0f, 0.40f, 0.6f}
};

//-----------------------------------------------------------------------------
// Constructor
//
// Initialise all state variables and allocate the delay buffers.  The
// default effect settings are: tone controls at 0 dB, modulation off,
// delay off.  The LFO phases and increments are initialised to zero and
// updated when a modulation preset is selected.
AudioEffectJPFX::AudioEffectJPFX()
    : AudioStream(2, NULL) // two inputs (left and right) by convention
{
    // Initialise tone control state and set default gains
    // Initialise stereo tone filters with unity gain and zero state
    auto initShelf = [&](ShelfFilter &f) {
        f.b0 = 1.0f;
        f.b1 = 0.0f;
        f.a1 = 0.0f;
        f.in1  = 0.0f;
        f.out1 = 0.0f;
    };
    initShelf(bassFilterL);
    initShelf(bassFilterR);
    initShelf(trebleFilterL);
    initShelf(trebleFilterR);
    targetBassGain   = 0.0f;
    targetTrebleGain = 0.0f;
    toneDirty        = true;

    // Set modulation defaults (modulation disabled)
    modType            = JPFX_MOD_OFF;
    modMix             = 0.5f;
    modRateOverride    = -1.0f;
    modFeedbackOverride= -1.0f;
    lfoPhaseL          = 0.0f;
    lfoPhaseR          = 0.5f; // start right channel 180° out of phase
    lfoIncL            = 0.0f;
    lfoIncR            = 0.0f;

    // Set delay defaults (delay disabled)
    delayType          = JPFX_DELAY_OFF;
    delayMix           = 0.5f;
    delayFeedbackOverride = -1.0f;
    delayTimeOverride     = -1.0f;

    // Allocate delay buffers
    allocateDelayBuffer();
}

//-----------------------------------------------------------------------------
// allocateDelayBuffer
//
// Allocate the circular delay buffers for the left and right channels.
// The buffer size is computed based on the maximum delay time specified
// by JPFX_MAX_DELAY_MS.  We convert the time to samples using the
// system sample rate.  If the allocation fails, the pointers are
// initialised to NULL and delay processing will silently disable itself.
void AudioEffectJPFX::allocateDelayBuffer()
{
    const float sr = AUDIO_SAMPLE_RATE_EXACT;
    // Convert milliseconds to samples and add 2 for linear interpolation
    float maxSamplesF = (JPFX_MAX_DELAY_MS * 0.001f * sr) + 2.0f;
    // Round up to nearest integer
    uint32_t samples  = (uint32_t)ceilf(maxSamplesF);
    delayBufSize      = samples;
    delayWriteIndex   = 0;
    // Allocate buffers; on Teensy 4.x with PSRAM the default malloc
    // automatically uses external memory when available.  If the
    // allocation fails, set pointers to nullptr.
    delayBufL = (float *)malloc(sizeof(float) * delayBufSize);
    delayBufR = (float *)malloc(sizeof(float) * delayBufSize);
    if (delayBufL == nullptr || delayBufR == nullptr) {
        // Allocation failure: fall back to no delay
        if (delayBufL) free(delayBufL);
        if (delayBufR) free(delayBufR);
        delayBufL = delayBufR = nullptr;
        delayBufSize = 0;
    } else {
        // Clear buffers
        for (uint32_t i = 0; i < delayBufSize; ++i) {
            delayBufL[i] = 0.0f;
            delayBufR[i] = 0.0f;
        }
    }
}

//-----------------------------------------------------------------------------
// computeShelfCoeffs
//
// Compute the coefficients for a first‑order shelving filter.  We use
// Robert Bristow‑Johnson's EQ Cookbook formulas for first‑order low‑
// shelf and high‑shelf filters.  The resulting difference equation is:
//   y[n] = b0*x[n] + b1*x[n-1] - a1*y[n-1]
// where b0, b1 and a1 are derived below.  The gain in dB is
// converted to a linear scale V0.  A corner frequency near 100 Hz is
// used for the bass shelf and around 4 kHz for the treble shelf.
void AudioEffectJPFX::computeShelfCoeffs(ShelfFilter &filt, float cornerHz, float gainDB, bool high)
{
    const float fs = AUDIO_SAMPLE_RATE_EXACT;
    // Convert gain from dB to linear amplitude
    float V0 = powf(10.0f, gainDB / 20.0f);
    float sqrtV0 = sqrtf(V0);
    // Prewarp the cutoff frequency using bilinear transform
    float K = tanf(M_PI * cornerHz / fs);
    // Compute coefficients for low or high shelf
    if (!high) {
        // Low shelf
        float b0 = (1.0f + sqrtV0 * K) / (1.0f + K);
        float b1 = (1.0f - sqrtV0 * K) / (1.0f + K);
        float a1 = (1.0f - K) / (1.0f + K);
        filt.b0  = b0;
        filt.b1  = b1;
        filt.a1  = a1;
    } else {
        // High shelf
        float b0 = (sqrtV0 + K) / (1.0f + K);
        float b1 = (sqrtV0 - K) / (1.0f + K);
        float a1 = (1.0f - K) / (1.0f + K);
        filt.b0  = b0;
        filt.b1  = b1;
        filt.a1  = a1;
    }
    // Reset filter state to avoid pops when coefficients change
    filt.in1  = 0.0f;
    filt.out1 = 0.0f;
}

//-----------------------------------------------------------------------------
// applyTone
//
// Apply the shelving filters to a stereo sample.  The filters are applied
// in series: bass (low‑shelf) followed by treble (high‑shelf).  The
// sample values are modified in place.  This function is inline for
// efficiency and is called once per sample from update().
inline void AudioEffectJPFX::applyTone(float &l, float &r)
{
    // Low‑shelf (bass) on left channel
    {
        float x0 = l;
        float y0 = bassFilterL.b0 * x0 + bassFilterL.b1 * bassFilterL.in1 - bassFilterL.a1 * bassFilterL.out1;
        bassFilterL.in1  = x0;
        bassFilterL.out1 = y0;
        l = y0;
    }
    // Low‑shelf (bass) on right channel
    {
        float x0 = r;
        float y0 = bassFilterR.b0 * x0 + bassFilterR.b1 * bassFilterR.in1 - bassFilterR.a1 * bassFilterR.out1;
        bassFilterR.in1  = x0;
        bassFilterR.out1 = y0;
        r = y0;
    }
    // High‑shelf (treble) on left channel
    {
        float x0 = l;
        float y0 = trebleFilterL.b0 * x0 + trebleFilterL.b1 * trebleFilterL.in1 - trebleFilterL.a1 * trebleFilterL.out1;
        trebleFilterL.in1  = x0;
        trebleFilterL.out1 = y0;
        l = y0;
    }
    // High‑shelf (treble) on right channel
    {
        float x0 = r;
        float y0 = trebleFilterR.b0 * x0 + trebleFilterR.b1 * trebleFilterR.in1 - trebleFilterR.a1 * trebleFilterR.out1;
        trebleFilterR.in1  = x0;
        trebleFilterR.out1 = y0;
        r = y0;
    }
}

//-----------------------------------------------------------------------------
// setBassGain
//
// Set the desired bass gain in decibels.  The actual filter coefficients
// will be updated lazily at the start of the next update() call by
// checking the toneDirty flag.  This avoids recalculating coefficients
// for every sample.
void AudioEffectJPFX::setBassGain(float dB)
{
    if (dB != targetBassGain) {
        targetBassGain = dB;
        toneDirty = true;
    }
}

//-----------------------------------------------------------------------------
// setTrebleGain
//
// Set the desired treble gain in decibels.  As with setBassGain(), the
// coefficients are updated lazily at the start of update().
void AudioEffectJPFX::setTrebleGain(float dB)
{
    if (dB != targetTrebleGain) {
        targetTrebleGain = dB;
        toneDirty = true;
    }
}

//-----------------------------------------------------------------------------
// setModEffect
//
// Select a new modulation preset.  If JPFX_MOD_OFF is passed, the
// modulation engine is disabled.  When a new preset is selected the LFO
// increments are recomputed based on the preset rate and any user
// overrides.
void AudioEffectJPFX::setModEffect(ModEffectType type)
{
    if (type != modType) {
        modType = type;
        // Reset LFO phases to avoid clicks when switching presets
        lfoPhaseL = 0.0f;
        lfoPhaseR = 0.5f;
        updateLfoIncrements();
    }
}

//-----------------------------------------------------------------------------
// setModMix
//
// Set the modulation wet/dry mix.  Values outside 0..1 are clamped.
void AudioEffectJPFX::setModMix(float mix)
{
    if (mix < 0.0f) mix = 0.0f;
    if (mix > 1.0f) mix = 1.0f;
    modMix = mix;
}

//-----------------------------------------------------------------------------
// setModRate
//
// Override the modulation LFO rate.  A value of zero disables the
// override and reverts to the preset rate.  Negative values are ignored.
void AudioEffectJPFX::setModRate(float rate)
{
    if (rate < 0.0f) {
        // ignore
        return;
    }
    if (rate == 0.0f) {
        modRateOverride = -1.0f;
    } else {
        modRateOverride = rate;
    }
    updateLfoIncrements();
}

//-----------------------------------------------------------------------------
// setModFeedback
//
// Override the modulation feedback.  Negative values disable the override.
void AudioEffectJPFX::setModFeedback(float fb)
{
    if (fb < 0.0f) {
        modFeedbackOverride = -1.0f;
    } else {
        if (fb > 0.99f) fb = 0.99f;
        modFeedbackOverride = fb;
    }
}

//-----------------------------------------------------------------------------
// setDelayEffect
//
// Select a new delay preset.  Passing JPFX_DELAY_OFF disables delay
// processing.  When a new preset is selected the delay write index is
// reset and the delay buffer cleared to avoid artefacts from the
// previous effect.
void AudioEffectJPFX::setDelayEffect(DelayEffectType type)
{
    if (type != delayType) {
        delayType = type;
        // Reset delay write pointer and clear buffers to prevent
        // bleeding from previous delay settings.
        delayWriteIndex = 0;
        if (delayBufL && delayBufR) {
            for (uint32_t i = 0; i < delayBufSize; ++i) {
                delayBufL[i] = 0.0f;
                delayBufR[i] = 0.0f;
            }
        }
    }
}

//-----------------------------------------------------------------------------
// setDelayMix
//
// Set the delay wet/dry mix.  Values outside −1..1 are clamped; negative
// values invert the wet signal (useful for comb filtering effects).  A
// default of 0.5 gives equal blend of dry and wet signals.
void AudioEffectJPFX::setDelayMix(float mix)
{
    if (mix < -1.0f) mix = -1.0f;
    if (mix >  1.0f) mix =  1.0f;
    delayMix = mix;
}

//-----------------------------------------------------------------------------
// setDelayFeedback
//
// Override the delay feedback amount.  Negative values disable the
// override.  Values above 0.99 are clamped to prevent runaway.
void AudioEffectJPFX::setDelayFeedback(float fb)
{
    if (fb < 0.0f) {
        delayFeedbackOverride = -1.0f;
    } else {
        if (fb > 0.99f) fb = 0.99f;
        delayFeedbackOverride = fb;
    }
}

//-----------------------------------------------------------------------------
// setDelayTime
//
// Override the delay time.  A value of zero reverts to the preset time.
// Negative values disable the override.  Times are clamped to
// JPFX_MAX_DELAY_MS to stay within the allocated buffer.
void AudioEffectJPFX::setDelayTime(float ms)
{
    if (ms < 0.0f) {
        delayTimeOverride = -1.0f;
    } else if (ms == 0.0f) {
        delayTimeOverride = -1.0f;
    } else {
        if (ms > JPFX_MAX_DELAY_MS) ms = JPFX_MAX_DELAY_MS;
        delayTimeOverride = ms;
    }
}

//-----------------------------------------------------------------------------
// updateLfoIncrements
//
// Recompute the LFO increments based on the current modulation preset and
// any rate override.  The LFO frequency for the right channel is set
// slightly higher to produce a subtle stereo spread.  When the
// modulation type is off, the increments are set to zero to avoid
// accumulating phase.
void AudioEffectJPFX::updateLfoIncrements()
{
    if (modType == JPFX_MOD_OFF) {
        lfoIncL = lfoIncR = 0.0f;
        return;
    }
    // Determine the base rate from preset
    float rate = modParams[modType].rate;
    // Override if requested
    if (modRateOverride > 0.0f) {
        rate = modRateOverride;
    }
    // Convert rate in Hz to phase increment per sample (radians)
    float fs = AUDIO_SAMPLE_RATE_EXACT;
    float phaseInc = 2.0f * 3.14159265358979323846f * rate / fs;
    // Slightly offset right channel for stereo width
    lfoIncL = phaseInc;
    lfoIncR = phaseInc * 1.01f;
}

//-----------------------------------------------------------------------------
// processModulation
//
// Apply the selected modulation effect to a single sample pair.  The
// routine reads from and writes into the delay buffer using a circular
// pointer.  Linear interpolation is used when reading the delayed
// samples to prevent aliasing when delay times are modulated.  The
// feedback parameter reinjects a portion of the processed signal
// back into the delay line.  Wet/dry mixing is applied based on the
// current modMix setting and the preset mix.  If modulation is off,
// this function returns the dry inputs.
inline void AudioEffectJPFX::processModulation(float inL, float inR, float &outL, float &outR)
{
    // If modulation disabled or no delay buffer allocated, bypass
    if (modType == JPFX_MOD_OFF || delayBufL == nullptr || delayBufR == nullptr) {
        outL = inL;
        outR = inR;
        return;
    }
    // Fetch parameters
    const ModParams &params = modParams[modType];
    float baseDelayL = params.baseDelayL;
    float baseDelayR = params.baseDelayR;
    float depthL     = params.depthL;
    float depthR     = params.depthR;
    float feedback   = (modFeedbackOverride >= 0.0f) ? modFeedbackOverride : params.feedback;
    float presetMix  = params.mix;
    float wetMix     = modMix * presetMix;
    float dryMix     = 1.0f - wetMix;
    // Compute LFO values (sinusoidal).  We use sinf() here; although
    // sinf() has some cost, the LFO frequency is very low so the call
    // overhead is negligible compared to the audio processing.  For
    // further optimisation you could implement a simple parabolic
    // approximation.  We update both phases for stereo spread.
    float lfoValL = sinf(lfoPhaseL);
    float lfoValR = sinf(lfoPhaseR);
    lfoPhaseL += lfoIncL;
    if (lfoPhaseL > 2.0f * 3.14159265358979323846f) lfoPhaseL -= 2.0f * 3.14159265358979323846f;
    lfoPhaseR += lfoIncR;
    if (lfoPhaseR > 2.0f * 3.14159265358979323846f) lfoPhaseR -= 2.0f * 3.14159265358979323846f;
    // Convert delay times from milliseconds to samples
    const float fs = AUDIO_SAMPLE_RATE_EXACT;
    float delaySamplesL = (baseDelayL + depthL * lfoValL) * 0.001f * fs;
    float delaySamplesR = (baseDelayR + depthR * lfoValR) * 0.001f * fs;
    // Clamp delay within buffer bounds
    if (delaySamplesL < 0.0f) delaySamplesL = 0.0f;
    if (delaySamplesR < 0.0f) delaySamplesR = 0.0f;
    if (delaySamplesL >= (float)(delayBufSize - 2)) delaySamplesL = (float)(delayBufSize - 2);
    if (delaySamplesR >= (float)(delayBufSize - 2)) delaySamplesR = (float)(delayBufSize - 2);
    // Compute read indices and interpolation fractions
    float readIndexL = (float)delayWriteIndex - delaySamplesL;
    if (readIndexL < 0.0f) readIndexL += (float)delayBufSize;
    uint32_t idxL0 = (uint32_t)readIndexL;
    uint32_t idxL1 = (idxL0 + 1) % delayBufSize;
    float fracL    = readIndexL - (float)idxL0;
    float delayedL = delayBufL[idxL0] + (delayBufL[idxL1] - delayBufL[idxL0]) * fracL;
    float readIndexR = (float)delayWriteIndex - delaySamplesR;
    if (readIndexR < 0.0f) readIndexR += (float)delayBufSize;
    uint32_t idxR0 = (uint32_t)readIndexR;
    uint32_t idxR1 = (idxR0 + 1) % delayBufSize;
    float fracR    = readIndexR - (float)idxR0;
    float delayedR = delayBufR[idxR0] + (delayBufR[idxR1] - delayBufR[idxR0]) * fracR;
    // Write into delay buffer with feedback
    float writeL = inL + delayedL * feedback;
    float writeR = inR + delayedR * feedback;
    delayBufL[delayWriteIndex] = writeL;
    delayBufR[delayWriteIndex] = writeR;
    // Advance write pointer
    delayWriteIndex++;
    if (delayWriteIndex >= delayBufSize) delayWriteIndex = 0;
    // Mix dry and wet signals
    outL = dryMix * inL + wetMix * delayedL;
    outR = dryMix * inR + wetMix * delayedR;
}

//-----------------------------------------------------------------------------
// processDelay
//
// Apply the selected delay effect to a single sample pair.  The delay
// engine uses the same delay buffers as the modulation effect but
// operates independently.  Each preset defines fixed delay times for
// the left and right channels as well as feedback and wet/dry mix.  A
// user override can replace the preset times and feedback.  Delay is
// applied after modulation so the two effects can be cascaded.
inline void AudioEffectJPFX::processDelay(float inL, float inR, float &outL, float &outR)
{
    // If delay disabled or no buffer allocated, bypass
    if (delayType == JPFX_DELAY_OFF || delayBufL == nullptr || delayBufR == nullptr) {
        outL = inL;
        outR = inR;
        return;
    }
    // Fetch parameters
    const DelayParams &params = delayParams[delayType];
    float delayTimeL = params.delayL;
    float delayTimeR = params.delayR;
    float feedback   = (delayFeedbackOverride >= 0.0f) ? delayFeedbackOverride : params.feedback;
    float wetMix     = delayMix;
    float dryMix     = 1.0f - fabsf(wetMix);
    bool invertWet   = (wetMix < 0.0f);
    wetMix = fabsf(wetMix);
    // Apply override to delay times if present
    if (delayTimeOverride >= 0.0f) {
        delayTimeL = delayTimeOverride;
        delayTimeR = delayTimeOverride;
    }
    // Convert delay times from ms to samples
    const float fs = AUDIO_SAMPLE_RATE_EXACT;
    float delaySamplesL = delayTimeL * 0.001f * fs;
    float delaySamplesR = delayTimeR * 0.001f * fs;
    // Clamp delay within buffer bounds
    if (delaySamplesL < 0.0f) delaySamplesL = 0.0f;
    if (delaySamplesR < 0.0f) delaySamplesR = 0.0f;
    if (delaySamplesL >= (float)(delayBufSize - 2)) delaySamplesL = (float)(delayBufSize - 2);
    if (delaySamplesR >= (float)(delayBufSize - 2)) delaySamplesR = (float)(delayBufSize - 2);
    // Compute read indices and interpolation fractions
    float readIndexL = (float)delayWriteIndex - delaySamplesL;
    if (readIndexL < 0.0f) readIndexL += (float)delayBufSize;
    uint32_t idxL0 = (uint32_t)readIndexL;
    uint32_t idxL1 = (idxL0 + 1) % delayBufSize;
    float fracL    = readIndexL - (float)idxL0;
    float delayedL = delayBufL[idxL0] + (delayBufL[idxL1] - delayBufL[idxL0]) * fracL;
    float readIndexR = (float)delayWriteIndex - delaySamplesR;
    if (readIndexR < 0.0f) readIndexR += (float)delayBufSize;
    uint32_t idxR0 = (uint32_t)readIndexR;
    uint32_t idxR1 = (idxR0 + 1) % delayBufSize;
    float fracR    = readIndexR - (float)idxR0;
    float delayedR = delayBufR[idxR0] + (delayBufR[idxR1] - delayBufR[idxR0]) * fracR;
    // Write into buffer with feedback
    float writeL = inL + delayedL * feedback;
    float writeR = inR + delayedR * feedback;
    delayBufL[delayWriteIndex] = writeL;
    delayBufR[delayWriteIndex] = writeR;
    // Advance write pointer
    delayWriteIndex++;
    if (delayWriteIndex >= delayBufSize) delayWriteIndex = 0;
    // Wet inversion (phase reversal) if requested
    float wetL = invertWet ? -delayedL : delayedL;
    float wetR = invertWet ? -delayedR : delayedR;
    // Mix dry and wet
    outL = dryMix * inL + wetMix * wetL;
    outR = dryMix * inR + wetMix * wetR;
}

//-----------------------------------------------------------------------------
// update
//
// The core processing routine called by the audio engine once per audio
// block.  It reads stereo input blocks (left and right), applies tone
// control, modulation and delay in sequence, then writes the results to
// new output blocks.  If any input block is missing or output blocks
// cannot be allocated, the routine returns early.  Tone coefficients
// are recalculated at the start of the block if necessary.
void AudioEffectJPFX::update(void)
{
    audio_block_t *inL  = receiveReadOnly(0);
    audio_block_t *inR  = receiveReadOnly(1);
    // If no input on either channel, release any allocated blocks and exit
    if (!inL || !inR) {
        if (inL) release(inL);
        if (inR) release(inR);
        return;
    }
    audio_block_t *outL = allocate();
    audio_block_t *outR = allocate();
    if (!outL || !outR) {
        if (outL) release(outL);
        if (outR) release(outR);
        release(inL);
        release(inR);
        return;
    }
    // Update tone filter coefficients if necessary
    if (toneDirty) {
        // Set corner frequencies approximating JP‑8000 tone control
        computeShelfCoeffs(bassFilterL, 100.0f, targetBassGain, false);
        computeShelfCoeffs(bassFilterR, 100.0f, targetBassGain, false);
        computeShelfCoeffs(trebleFilterL, 4000.0f, targetTrebleGain, true);
        computeShelfCoeffs(trebleFilterR, 4000.0f, targetTrebleGain, true);
        toneDirty = false;
    }
    // Process each sample in the block
    for (int i = 0; i < AUDIO_BLOCK_SAMPLES; ++i) {
        // Convert 16‑bit inputs to floats in [−1,1]
        float l = (float)inL->data[i] * (1.0f / 32768.0f);
        float r = (float)inR->data[i] * (1.0f / 32768.0f);
        // Apply tone EQ
        applyTone(l, r);
        // Apply modulation
        float modL, modR;
        processModulation(l, r, modL, modR);
        // Apply delay
        float delL, delR;
        processDelay(modL, modR, delL, delR);
        // Convert back to 16‑bit and clip
        float clL = delL;
        float clR = delR;
        if (clL > 1.0f) clL = 1.0f;
        if (clL < -1.0f) clL = -1.0f;
        if (clR > 1.0f) clR = 1.0f;
        if (clR < -1.0f) clR = -1.0f;
        outL->data[i] = (int16_t)(clL * 32767.0f);
        outR->data[i] = (int16_t)(clR * 32767.0f);
    }
    // Transmit the processed blocks to the next stage
    transmit(outL, 0);
    transmit(outR, 1);
    release(outL);
    release(outR);
    // Release input blocks
    release(inL);
    release(inR);
}
