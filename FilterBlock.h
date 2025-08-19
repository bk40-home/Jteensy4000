#pragma once
#include <Audio.h>


class FilterBlock {
public:
    FilterBlock();

    void setCutoff(float freqHz);
    void setResonance(float amount);
    void setDrive(float amount);
    void setOctaveControl(float octaves);
    void setPassbandGain(float gain);

    void setEnvModAmount(float amount);
    void setKeyTrackAmount(float amount);

    float getCutoff() const;
    float getResonance() const;
    float getDrive() const;
    float getOctaveControl() const;
    float getPassbandGain() const;
    float getEnvModAmount() const;
    float getKeyTrackAmount() const;

    AudioStream& input();
    AudioStream& output();
    AudioStream& envmod();
    AudioMixer4& modMixer();

private:
    AudioFilterLadder _filter;
    AudioMixer4 _modMixer;
    AudioSynthWaveformDc _envModDc; // going to patch this to the input of the Filter envelope
    AudioSynthWaveformDc _keyTrackDc;

    float _cutoff = 0.0f;
    float _resonance = 0.0f;
    float _drive = 1.0f;
    float _octaveControl = 4.0f;
    float _passbandGain = 1.0f;
    float _envModAmount = 0.0f;
    float _keyTrackAmount = 0.0f;

    AudioConnection* _patchCables[2];
};
