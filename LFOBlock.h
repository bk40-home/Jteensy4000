#pragma once
#include <Arduino.h>
#include <Audio.h>

enum LFODestination {
    LFO_DEST_NONE = 0,
    LFO_DEST_PITCH,
    LFO_DEST_FILTER,
    LFO_DEST_PWM,
    LFO_DEST_AMP,
    NUM_LFO_DESTS
};


class LFOBlock {
public:
    // --- Lifecycle
    LFOBlock();
    void update();

    // --- Parameter Setters
    void setWaveformType(int type);
    void setFrequency(float freq);
    void setAmplitude(float amp);
    void setDestination(LFODestination destination);

    // --- Parameter Getters
    float getFrequency() const;
    float getAmplitude() const;
    int getWaveform() const;
    LFODestination getDestination() const;
    // --- Outputs
    AudioStream& output();

private:
    int _type = 0;
    float _freq = 1.0f;
    float _amp = 0.0f;
    AudioSynthWaveform _lfo;
    LFODestination _destination = LFO_DEST_NONE;
};
