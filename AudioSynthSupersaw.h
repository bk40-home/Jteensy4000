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

    float detuneCurve(float x);
    void calculateIncrements();
    void calculateGains();
    void calculateHPF();

};
