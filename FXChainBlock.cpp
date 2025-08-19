#include <stdint.h>
#include "FXChainBlock.h"

FXChainBlock::FXChainBlock()
    : _mixerDelayIn(), _delay8taps(), _freeverb(),
      _mixerDelayOutL(), _mixerDelayOutR(),
      _mixerOutL(), _mixerOutR()
{
    // Patch routing
    patchCords[0]  = new AudioConnection(_ampIn, _freeverb);
    patchCords[1]  = new AudioConnection(_ampIn, 0, _mixerOutL, 0);
    patchCords[2]  = new AudioConnection(_ampIn, 0, _mixerOutR, 0);
    patchCords[3]  = new AudioConnection(_ampIn, 0, _mixerDelayIn, 0);

    patchCords[4]  = new AudioConnection(_mixerDelayIn, _delay8taps);

    patchCords[5]  = new AudioConnection(_delay8taps, 0, _mixerDelayOutL, 0);
    patchCords[6]  = new AudioConnection(_delay8taps, 2, _mixerDelayOutL, 1);
    patchCords[7]  = new AudioConnection(_delay8taps, 4, _mixerDelayOutL, 2);
    patchCords[8]  = new AudioConnection(_delay8taps, 6, _mixerDelayOutL, 3);
    patchCords[9]  = new AudioConnection(_delay8taps, 1, _mixerDelayOutR, 0);
    patchCords[10] = new AudioConnection(_delay8taps, 3, _mixerDelayOutR, 1);
    patchCords[11] = new AudioConnection(_delay8taps, 5, _mixerDelayOutR, 2);
    patchCords[12] = new AudioConnection(_delay8taps, 7, _mixerDelayOutR, 3);

    patchCords[13] = new AudioConnection(_freeverb, 0, _mixerOutR, 1);
    patchCords[14] = new AudioConnection(_freeverb, 0, _mixerOutL, 1);
    patchCords[15] = new AudioConnection(_freeverb, 0, _mixerDelayIn, 1);

    patchCords[16] = new AudioConnection(_mixerDelayOutL, 0, _mixerOutL, 2);
    patchCords[17] = new AudioConnection(_mixerDelayOutL, 0, _mixerDelayIn, 2);
    patchCords[18] = new AudioConnection(_mixerDelayOutR, 0, _mixerOutR, 2);
    patchCords[19] = new AudioConnection(_mixerDelayOutR, 0, _mixerDelayIn, 3);

    // Default mixer gains
    setDryMix(1.0f, 1.0f);
    setReverbMix(0.0f, 0.0f);
    setDelayMix(0.0f, 0.0f);
    _mixerDelayIn.gain(0,1);
    _mixerDelayIn.gain(1,0);
    _mixerDelayIn.gain(2,0);
    _mixerDelayIn.gain(3,0);

    for (uint8_t c = 0 ; c < 4; c++) {
        float g = (1 - (c/10));
        _mixerDelayOutL.gain(c,g);
        _mixerDelayOutR.gain(c,g);
    }

}

// --- Reverb control ---

void FXChainBlock::setReverbRoomSize(float size) {
    _roomSize = size;
    _freeverb.roomsize(size);
}

void FXChainBlock::setReverbDamping(float damping) {
    _damping = damping;
    _freeverb.damping(damping);
}

float FXChainBlock::getReverbRoomSize() const {
    return _roomSize;
}

float FXChainBlock::getReverbDamping() const {
    return _damping;
}

float FXChainBlock::getDelayFeedback() const {
    return _delayFeedback;
}

// --- Delay control ---

void FXChainBlock::setBaseDelayTime(float baseMs) {
    float safeBase = constrain(baseMs, 5.0f, 1000.0f);
    for (uint8_t i = 0; i < 8; ++i) {
        float offset = i * safeBase * 0.5f; // for example, 0.5x steps
        setDelayTapTime(i, offset);
    }
}

void FXChainBlock::setDelayTapTime(uint8_t tap, float timeMs) {
    if (tap < 8) {
        float safeTime = constrain(timeMs, 5.0f, 2000.0f);  // 5ms–1500ms recommended
        _delayTapTimes[tap] = safeTime;
        _delay8taps.delay(tap, safeTime);
        Serial.printf("tap %d time = %.2f\n", tap, safeTime);
    }
}

float FXChainBlock::getDelayTapTime(uint8_t tap) const {
    return (tap < 8) ? _delayTapTimes[tap] : 0.0f;

}

// --- Mixer control ---

void FXChainBlock::setDryMix(float levelL, float levelR) {
    _dryMixL = levelL;
    _dryMixR = levelR;
    _mixerOutL.gain(0, levelL);
    _mixerOutR.gain(0, levelR);
}

void FXChainBlock::setReverbMix(float levelL, float levelR) {
    _reverbMixL = levelL;
    _reverbMixR = levelR;
    _mixerOutL.gain(1, levelL);
    _mixerOutR.gain(1, levelR);
}

void FXChainBlock::setDelayMix(float levelL, float levelR) {
    _delayMixL = levelL;
    _delayMixR = levelR;
    _mixerOutL.gain(2, levelL);
    _mixerOutR.gain(2, levelR);


}

void FXChainBlock::setDelayFeedback(float delayFeedback) {
    _delayFeedback = delayFeedback;
    _mixerDelayIn.gain(2, _delayFeedback);
    _mixerDelayIn.gain(3, _delayFeedback);


}

// --- Access outputs ---

AudioMixer4& FXChainBlock::getOutputLeft() {
    return _mixerOutL;
}

AudioMixer4& FXChainBlock::getOutputRight() {
    return _mixerOutR;
}
