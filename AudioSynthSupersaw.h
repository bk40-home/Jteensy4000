#pragma once

#include <Arduino.h>
#include "AudioStream.h"

#define SUPERSAW_VOICES 7

class AudioSynthSupersaw : public AudioStream {
public:
    AudioSynthSupersaw();

    void setFrequency(float freq);
    void setAmplitude(float amp);
    void setDetune(float amount);
    void setMix(float mix);
    void setOutputGain(float gain);
    /**
     * Enable or disable 2× oversampling of the supersaw.  When enabled,
     * the oscillator generates two internal samples for each output sample
     * and low-pass averages them.  This reduces aliasing at the cost of
     * CPU.  Disabled by default.
     */
    void setOversample(bool enable);

    /**
     * Set the amount of slow per‑voice pitch drift (0..1).  A value of
     * 0 disables drift.  Increasing this value introduces subtle
     * low‑frequency modulation on each oscillator’s frequency to recreate
     * the beating and movement of real analogue oscillators.  Internally
     * this scales a triangular LFO to ±0.5 % of the base frequency.
     */
    void setDriftAmount(float amount);

    /**
     * Set the high‑pass filter cutoff frequency in Hz.  This filter
     * removes DC and very low frequency rumble from the summed saws.  The
     * default is 30 Hz.  Values below 1 Hz and above 2000 Hz are
     * clamped.  Setting this cutoff too high will remove the fundamental
     * of low notes.  When changed, the filter coefficient is updated
     * immediately.
     */
    void setHpfCutoff(float cutoff);
    void noteOn();

    virtual void update(void) override;

private:
    float freq;
    float detuneAmt;
    float mixAmt;
    float amp;
    float outputGain;
    float phases[SUPERSAW_VOICES];
    float phaseInc[SUPERSAW_VOICES];
    float gains[SUPERSAW_VOICES];
    float hpfPrevIn;
    float hpfPrevOut;
    float hpfAlpha;

    // Whether 2× internal oversampling is active
    bool oversample2x = false;
    // Amount of per‑voice drift (0..1)
    float driftAmt = 0.0f;
    // Per‑voice drift LFO phase (0..1)
    float lfoPhases[SUPERSAW_VOICES];
    // Per‑voice drift LFO increment per sample
    float lfoInc[SUPERSAW_VOICES];
    // HPF cutoff frequency
    float hpfCutoff = 30.0f;

    float detuneCurve(float x);
    void calculateIncrements();
    void calculateGains();
    void calculateHPF();

};
