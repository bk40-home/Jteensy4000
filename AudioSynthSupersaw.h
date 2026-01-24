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
     * Enable or disable internal 2× oversampling. When enabled the supersaw
     * renders twice as many internal samples and low‑pass filters before
     * decimating to the audio engine sample rate. This reduces aliasing
     * artifacts, especially at high detune settings. Disabled by default.
     */
    void setOversample(bool enable);

    /**
     * Set the amount of slow, per‑voice pitch drift. A value of 0.0f (the
     * default) disables drift entirely. Increasing this value introduces a
     * gentle, independent LFO on each oscillator’s frequency, giving
     * additional beating and liveliness at low detune amounts. Values
     * typically range from 0.0f to 1.0f. Internally this scales a maximum
     * drift of around ±0.5 % of the base frequency.
     */
    void setDriftAmount(float amount);

    /**
     * Set the cutoff frequency of the one‑pole high‑pass filter used to
     * remove DC and low‑frequency bias from the summed waveform. By default
     * this cutoff is fixed at 30 Hz, preserving the fundamental and low
     * beating frequencies. The value is clamped to the range 1 Hz to
     * 2000 Hz. Note that extremely high cutoff values will drastically
     * brighten the sound and remove the fundamental. The cutoff is applied
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

    // Flag controlling whether 2× internal oversampling is active.
    bool oversample2x;
    // Amount of per‑voice frequency drift. 0.0f disables drift.
    float driftAmt;
    // Per‑voice LFO phase for drift modulation.
    float lfoPhases[SUPERSAW_VOICES];
    // Per‑voice LFO increment per sample.
    float lfoInc[SUPERSAW_VOICES];
    // High‑pass filter cutoff in Hz.
    float hpfCutoff;

    float detuneCurve(float x);
    void calculateIncrements();
    void calculateGains();
    void calculateHPF();

};