#pragma once

#include <Audio.h>

class FXChainBlock {
public:
    FXChainBlock();

    // --- Public control API ---
    void setReverbRoomSize(float size);
    void setReverbDamping(float damping);
    float getReverbRoomSize() const;
    float getReverbDamping() const;
    float getDelayFeedback() const;

    void setBaseDelayTime(float baseMs);
    void setDelayTapTime(uint8_t tap, float timeMs);
    float getDelayTapTime(uint8_t tap) const;

    void setDryMix(float levelL, float levelR);
    void setReverbMix(float levelL, float levelR);
    void setDelayMix(float levelL, float levelR);
    void setDelayFeedback(float _delayFeedback);

    // Output access for patching
    AudioMixer4& getOutputLeft();
    AudioMixer4& getOutputRight();

    // Input connection for summed voices
    AudioAmplifier _ampIn;

private:
    // Audio components
    AudioMixer4 _mixerDelayIn;
    AudioEffectDelay _delay8taps;
    AudioEffectFreeverb _freeverb;
    AudioMixer4 _mixerDelayOutL, _mixerDelayOutR;
    AudioMixer4 _mixerOutL, _mixerOutR;

    // Patch cords
    AudioConnection* patchCords[20];

    // Internal state for UI and CC access
    float _roomSize = 0.5f;
    float _damping = 0.5f;
    float _delayTapTimes[8] = {0};
    float _dryMixL = 1.0f, _dryMixR = 1.0f;
    float _reverbMixL = 0.0f, _reverbMixR = 0.0f;
    float _delayMixL = 0.0f, _delayMixR = 0.0f;
    float _delayFeedback = 0.0f;
};


