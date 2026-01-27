/*
 * AudioEffectJPFX.h
 *
 * This module implements the effects section of the Roland JP‑8000 synthesizer
 * for the JT‑4000 project.  According to the reverse‑engineered ROM and
 * original documentation, the JP‑8000 offers a simple tone control (bass
 * and treble), a modulation effect block with eleven variations (chorus,
 * flanger and phaser), and a delay block with five variations【536569142423667†L218-L224】.  The
 * class below combines these three effect types into a single audio object
 * compatible with the Teensy audio library.  Effects can be enabled
 * independently, and each variation is defined by a small set of parameters
 * chosen to approximate the original hardware.  CPU usage is kept low by
 * sharing a single modulated delay engine for chorus/flanger/phaser and a
 * simple circular buffer for delay lines.  PSRAM can be used to store the
 * delay buffer when longer times are needed.
 *
 * The design goals for this effect module are:
 *   - Faithfully reproduce the JP‑8000 tone, modulation and delay effects.
 *   - Remain fully compatible with the Teensy Audio library (inherit
 *     AudioStream and implement update() accordingly).
 *   - Provide a clean and clear API for selecting effect variations and
 *     adjusting basic parameters (mix, feedback, rate, etc.).
 *   - Comment all code for ease of maintenance and future development.
 *   - Avoid intrusive changes to the rest of the JT‑4000 codebase.
 */

#pragma once

#include <Arduino.h>
#include "AudioStream.h"

// Maximum delay time in milliseconds.  The JP‑8000's delay extends up to
// 1250ms【536569142423667†L218-L224】; we allocate a little extra headroom.  Changing this
// constant will directly affect the size of the delay buffer allocated at
// construction time.  If PSRAM is available it will be used to allocate
// this buffer.
#define JPFX_MAX_DELAY_MS    1500.0f

// Number of modulation effect variations (chorus/flanger/phaser) on the
// original JP‑8000.  These are parameter presets rather than discrete
// algorithms.  See modParams[] in the .cpp file for details.
#define JPFX_NUM_MOD_VARIATIONS 11

// Number of delay effect variations on the original JP‑8000.  Again, these
// are presets defined in delayParams[].
#define JPFX_NUM_DELAY_VARIATIONS 5

// Forward declaration of AudioEffectJPFX for use in inline helper functions.
class AudioEffectJPFX;

/*
 * AudioEffectJPFX
 *
 * This class derives from AudioStream and implements the JP‑8000 FX section.
 * It exposes a simple API to select modulation and delay variations and to
 * adjust parameters such as mix, feedback, rate and tone control.  Internally
 * it uses:
 *  - A pair of first‑order shelf filters for bass and treble tone control.
 *  - A modulated delay line for chorus/flanger/phaser variations.
 *  - A stereo delay buffer for delay variations with optional panning.
 *
 * One instance of this class can service both channels of a stereo signal
 * (left and right).  The update() method processes audio frames in blocks
 * of AUDIO_BLOCK_SAMPLES samples, as required by the Teensy audio library.
 */
class AudioEffectJPFX : public AudioStream {
public:
    // Enumeration for selecting the modulation effect variation.  Values
    // correspond to indexes into the modParams array defined in the
    // implementation file.  JPFX_MOD_OFF disables modulation entirely.
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

    // Enumeration for selecting the delay effect variation.  Values
    // correspond to indexes into the delayParams array defined in the
    // implementation file.  JPFX_DELAY_OFF disables delay processing.
    enum DelayEffectType {
        JPFX_DELAY_OFF = -1,
        JPFX_DELAY_SHORT = 0,
        JPFX_DELAY_LONG,
        JPFX_DELAY_PINGPONG1,
        JPFX_DELAY_PINGPONG2,
        JPFX_DELAY_PINGPONG3
    };

    // Constructor.  The AudioEffectJPFX object has one input and one output
    // channel (stereo processing is handled internally).  The constructor
    // allocates delay buffers based on JPFX_MAX_DELAY_MS and the system
    // sample rate.  If PSRAM is present it will be used automatically.
    AudioEffectJPFX();

    // Override update() from AudioStream.  This method is called
    // periodically by the audio engine to process blocks of audio.  It
    // applies tone control, modulation and delay according to current
    // settings.  The implementation is in the .cpp file.
    virtual void update(void) override;

    // ----- Tone control interface -----

    // Set the bass gain in decibels.  Positive values boost low
    // frequencies, negative values attenuate them.  The range is
    // approximately ±12 dB.  The default is 0 dB.
    void setBassGain(float dB);

    // Set the treble gain in decibels.  Positive values boost high
    // frequencies, negative values attenuate them.  The range is
    // approximately ±12 dB.  The default is 0 dB.
    void setTrebleGain(float dB);

    // ----- Modulation effect interface -----

    // Select which modulation variation to use.  Passing JPFX_MOD_OFF
    // disables the modulation engine.  The enumeration values map onto
    // specific parameter presets defined in the .cpp file.
    void setModEffect(ModEffectType type);

    // Set the modulation mix.  When mix=0.0f the effect is dry and the
    // processed output is not heard.  When mix=1.0f the signal is
    // completely wet (only processed output).  Intermediate values blend
    // the dry and wet signals.  Default is 0.5f.
    void setModMix(float mix);

    // Set a global LFO rate override for modulation effects in hertz.
    // If set to a positive value, it overrides the built‑in rate for
    // whichever modulation variation is selected.  A value of 0.0f means
    // use the preset rate.  Note that extremely high rates (>10 Hz) may
    // produce metallic artefacts.
    void setModRate(float rate);

    // Optional global feedback override for modulation effects (0.0f to
    // 0.99f).  A value of -1.0f uses the preset feedback for the selected
    // variation.  Feedback is primarily used by flanger and phaser
    // presets; chorus presets generally leave this at zero.
    void setModFeedback(float fb);

    // ----- Delay effect interface -----

    // Select which delay variation to use.  Passing JPFX_DELAY_OFF
    // disables the delay engine.  The enumeration values map onto
    // specific parameter presets defined in the .cpp file.
    void setDelayEffect(DelayEffectType type);

    // Set the delay wet/dry mix.  When mix=0.0f the output is dry only;
    // when mix=1.0f the output is fully wet (only delay).  Default is
    // 0.5f for an equal blend.  Negative values invert the wet signal.
    void setDelayMix(float mix);

    // Set a global delay feedback override (0.0f to 0.99f).  A value of
    // -1.0f uses the preset feedback for the selected delay variation.
    void setDelayFeedback(float fb);

    // Set a global delay time override in milliseconds.  A value of
    // 0.0f uses the preset delay times for the selected variation.  This
    // parameter is especially useful when synchronising delay to MIDI
    // clock or the internal arpeggiator; simply convert note values into
    // milliseconds before calling this function.  Values exceeding
    // JPFX_MAX_DELAY_MS are clamped.  Negative values disable the
    // override.
    void setDelayTime(float ms);

private:
    // ----- Tone control internals -----

    // Structure to hold coefficients and state for a single first‑order
    // shelving filter.  We implement bass and treble tone control with
    // simple biquad‑like filters: a low‑shelf and a high‑shelf.  Each
    // filter has its own state and coefficients.
    typedef struct {
        /*
         * Coefficients and state for a first‑order shelving filter.  We
         * implement the filter using a direct form I realisation:
         *   y[n] = b0*x[n] + b1*x[n-1] - a1*y[n-1]
         * Coefficients b0, b1 and a1 are derived from the desired
         * corner frequency and gain.  The variables in1 and out1
         * store the previous input and output samples for the filter.
         */
        float b0;    // feedforward coefficient on current input
        float b1;    // feedforward coefficient on previous input
        float a1;    // feedback coefficient on previous output
        float in1;   // x[n-1] state
        float out1;  // y[n-1] state
    } ShelfFilter;

    // Compute filter coefficients for a shelving filter given the corner
    // frequency and gain in decibels.  Positive gain creates a boost,
    // negative gain creates a cut.  The calculations are derived from the
    // Audio EQ Cookbook by Robert Bristow‑Johnson.
    void computeShelfCoeffs(ShelfFilter &filt, float cornerHz, float gainDB, bool high);

    // Tone control filter instances.  The bass filter is a low‑shelf
    // boosting or cutting around ~100 Hz.  The treble filter is a
    // high‑shelf boosting or cutting around ~4 kHz.  Corner frequencies are
    // chosen to approximate the JP‑8000's tone controls.
    // Separate filters for left and right channels.  Each stereo
    // channel has its own low‑shelf (bass) and high‑shelf (treble)
    // filter instances.  Using independent state per channel avoids
    // crosstalk when filtering.
    ShelfFilter bassFilterL;
    ShelfFilter bassFilterR;
    ShelfFilter trebleFilterL;
    ShelfFilter trebleFilterR;

    // Desired bass and treble gains.  Setting these triggers recalculation
    // of the filter coefficients during the next update() call.
    float targetBassGain;
    float targetTrebleGain;
    bool toneDirty;

    // Apply the shelving filters to a stereo input sample.  Takes left and
    // right floats, modifies them in place.  This method is small and
    // inline for performance.
    inline void applyTone(float &l, float &r);

    // ----- Modulation effect internals -----

    // Structure defining a preset for modulation effects.  Each preset
    // corresponds to one of the 11 chorus/flanger/phaser variations.  The
    // parameters are explained in the implementation file.
    typedef struct {
        float baseDelayL;   // Base delay in milliseconds for left channel
        float baseDelayR;   // Base delay in milliseconds for right channel
        float depthL;       // Modulation depth in milliseconds for left
        float depthR;       // Modulation depth in milliseconds for right
        float rate;         // LFO rate in hertz
        float feedback;     // Feedback amount (0.0f..0.99f)
        float mix;          // Wet/dry mix (0.0f..1.0f)
        bool  isPhaser;     // Use all‑pass network instead of delay
        bool  isFlanger;    // Use shorter delay times
    } ModParams;

    // Array of modulation presets.  The order matches the ModEffectType
    // enumeration.  See the .cpp file for details.  If you wish to tweak
    // the JP‑8000 sound, these values are a good starting point.
    static const ModParams modParams[JPFX_NUM_MOD_VARIATIONS];

    // Selected modulation effect and overrides.  Negative values disable
    // the override and revert to the preset.  The LFO and feedback
    // overrides allow the user to customise the modulation independently
    // of the preset.  The mix parameter can be set globally via
    // setModMix().
    ModEffectType modType;
    float modMix;
    float modRateOverride;
    float modFeedbackOverride;

    // LFO phase for modulation.  The LFO is a simple sinusoid computed
    // per sample.  We precompute the increment from the rate to reduce
    // cost.  Use two phases (one per channel) for subtle stereo spread.
    float lfoPhaseL;
    float lfoPhaseR;
    float lfoIncL;
    float lfoIncR;

    // ----- Delay effect internals -----

    // Structure defining a preset for delay effects.  Each preset
    // corresponds to one of the 5 delay variations.  The parameters are
    // explained in the implementation file.
    typedef struct {
        float delayL;    // Delay time in milliseconds for left channel
        float delayR;    // Delay time in milliseconds for right channel
        float feedback;  // Feedback amount (0.0f..0.99f)
        float mix;       // Wet/dry mix (0.0f..1.0f)
    } DelayParams;

    // Array of delay presets.  The order matches the DelayEffectType
    // enumeration.  See the .cpp file for details.  These values
    // approximate the JP‑8000's delay options【536569142423667†L218-L224】.
    static const DelayParams delayParams[JPFX_NUM_DELAY_VARIATIONS];

    // Selected delay effect and overrides.  Negative values disable the
    // override and revert to the preset.
    DelayEffectType delayType;
    float delayMix;
    float delayFeedbackOverride;
    float delayTimeOverride;

    // Circular delay buffer pointers.  Each channel has its own buffer.
    float *delayBufL;
    float *delayBufR;
    uint32_t delayBufSize;
    uint32_t delayWriteIndex;

    // ----- Internal helper methods -----

    // Allocate the delay buffers.  This is called from the constructor.
    // If PSRAM is available, it will be used automatically.  The buffer
    // size depends on JPFX_MAX_DELAY_MS and the current sample rate.
    void allocateDelayBuffer();

    // Compute the LFO increment values based on the selected preset or
    // user override.  Called whenever a modulation preset or rate
    // override changes.
    void updateLfoIncrements();

    // Process the modulation effect for a single sample pair.  The
    // function takes the dry input samples (inL, inR) and returns the
    // processed left and right samples via outL and outR.  It uses the
    // selected preset and applies the current LFO phase and feedback.
    inline void processModulation(float inL, float inR, float &outL, float &outR);

    // Process the delay effect for a single sample pair.  The function
    // takes the dry input samples (inL, inR) and returns the processed
    // left and right samples via outL and outR.  It uses the selected
    // preset and the delay buffer.  The write pointer is advanced inside
    // this call.
    inline void processDelay(float inL, float inR, float &outL, float &outR);
};
