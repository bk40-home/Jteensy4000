#pragma once

#include <Audio.h>

// Include HexeFX integer-based effects for modulated ping-pong delay and plate reverb.
// These headers live in the hexefx_audiolib_i16 library that you've added to the project.
// The ping-pong delay can allocate its buffer in external PSRAM when requested
// via the constructor. The plate reverb uses internal DMAMEM for its delay lines.
#include "effect_delaystereo_i16.h"
#include "effect_platereverb_i16.h"

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
    // Replace the original 8‑tap delay and freeverb with a stereo modulated
    // ping‑pong delay feeding into a plate reverb.  Both are integer‑based
    // effects from the hexefx_audiolib_i16 library.  The delay is allocated
    // with a configurable maximum time in milliseconds and an option to
    // allocate its buffer in external PSRAM (enabled here).  The plate
    // reverb manages its own internal memory.
    AudioEffectDelayStereo_i16 _pingPongDelay;
    AudioEffectPlateReverb_i16 _plateReverb;
    // Final output mixers (left/right).  Input 0: dry signal from _ampIn.
    // Input 1: wet output of the ping‑pong delay (pre‑reverb).
    // Input 2: wet output of the plate reverb (post‑delay).
    AudioMixer4  _mixerOutL, _mixerOutR;

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

    // Maximum delay time for the ping‑pong delay in milliseconds.  The
    // constructor will allocate enough memory to cover this range.  Values
    // above ~500 ms require PSRAM; here we set 2000 ms to allow up to 2 seconds
    // of delay when external PSRAM is available on a Teensy 4.1.  If you
    // adjust this value, update the initialization list in the .cpp file.
    static constexpr uint32_t _maxDelayMs = 10000;
};


