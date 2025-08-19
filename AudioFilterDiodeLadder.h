#pragma once

#include <Arduino.h>
#include "AudioStream.h"

class AudioFilterDiodeLadder : public AudioStream {
public:
    AudioFilterDiodeLadder();
    void setCutoff(float cutoff);
    void setResonance(float resonance);

    void setSaturation(float saturation);
    void setCutoffMod(float mod); // new modulation setter
    virtual void update(void) override;

private:
    audio_block_t *inputQueueArray[2];
    float _sampleRate;
    float _cutoff;
    float _resonance;
    float _cutoffMod;
    float _saturation;
    float _z1[2][4];
    float _alpha[4], _beta[4], _a0[4], _fdbk[4];
    float _G[4];
    float _gamma_sum;

    void updateCoefficients(float freq);
    inline float processOnePole(int stage, float input, int channel);
};

