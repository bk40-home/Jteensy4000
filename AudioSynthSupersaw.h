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
    // Enable or disable 2× oversampling. When enabled the supersaw oscillator
    // generates two internal samples per output sample and averages them to
    // reduce aliasing at the cost of CPU. Disabled by default.
    void setOversample(bool enable);
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

    // Flag indicating whether 2× oversampling is active
    bool oversample2x;

    float detuneCurve(float x);
    void calculateIncrements();
    void calculateGains();
    void calculateHPF();

};
