#pragma once
#include <Arduino.h>
#include <Audio.h>
#include "Waveforms.h"  // ✅ use the same waveform IDs & names as main osc


enum LFODestination {
    LFO_DEST_NONE = 0,
    LFO_DEST_PITCH,
    LFO_DEST_FILTER,
    LFO_DEST_PWM,
    LFO_DEST_AMP,
    NUM_LFO_DESTS
};

// LFO destination names — indices must match LFODestination
static const char* LFODestNames[NUM_LFO_DESTS]
    __attribute__((unused)) = {
    "None",           // LFO_DEST_NONE
    "Pitch",          // LFO_DEST_PITCH
    "Filter",         // LFO_DEST_FILTER
    "Pulse Width",    // LFO_DEST_PWM   (called “PWM” in enum; UI shows “Pulse Width”)
    "Amp"             // LFO_DEST_AMP
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
