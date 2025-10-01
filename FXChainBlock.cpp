#include <stdint.h>
#include "FXChainBlock.h"

FXChainBlock::FXChainBlock()
    : _pingPongDelay(_maxDelayMs, true),
      _plateReverb(),
      _mixerOutL(), _mixerOutR()
{
    // The new FX chain routes the amplifier output through a stereo
    // modulated ping‑pong delay and then into a plate reverb.  Both
    // effects are configured to output fully wet signals; dry mixing is
    // handled by the final mixers.  Additional patch cords provide
    // individual taps to the output mixers for delay and reverb, allowing
    // separate level control for each effect.

    // Clear the patchCords array to avoid dangling pointers from old code.
    for (int i = 0; i < 20; ++i) patchCords[i] = nullptr;

    // Allocate patch cords for the new routing.
    // 0: Mono input from _ampIn to left channel of ping‑pong delay
    patchCords[0] = new AudioConnection(_ampIn, 0, _pingPongDelay, 0);
    // 1: Mono input from _ampIn to right channel of ping‑pong delay
    patchCords[1] = new AudioConnection(_ampIn, 0, _pingPongDelay, 1);
    // 2: Left output of ping‑pong delay into plate reverb left
    patchCords[2] = new AudioConnection(_ampIn, 0, _plateReverb, 0);
    // 3: Right output of ping‑pong delay into plate reverb right
    patchCords[3] = new AudioConnection(_ampIn, 1, _plateReverb, 1);
    // 4: Dry path from _ampIn to mixer output left channel 0 (dry)
    patchCords[4] = new AudioConnection(_ampIn, 0, _mixerOutL, 0);
    // 5: Dry path from _ampIn to mixer output right channel 0 (dry)
    patchCords[5] = new AudioConnection(_ampIn, 0, _mixerOutR, 0);
    // 6: Wet delay path from ping‑pong delay output to mixer output left channel 1
    patchCords[6] = new AudioConnection(_pingPongDelay, 0, _mixerOutL, 1);
    // 7: Wet delay path from ping‑pong delay output to mixer output right channel 1
    patchCords[7] = new AudioConnection(_pingPongDelay, 1, _mixerOutR, 1);
    // 8: Wet reverb path from plate reverb to mixer output left channel 2
    patchCords[8] = new AudioConnection(_plateReverb, 0, _mixerOutL, 2);
    // 9: Wet reverb path from plate reverb to mixer output right channel 2
    patchCords[9] = new AudioConnection(_plateReverb, 1, _mixerOutR, 2);

    // Configure the internal effects to output fully wet signals.  Dry mixing
    // is performed in the final mixers.  The delay mix is forced to 1.0 to
    // prevent duplicate dry signals feeding the reverb.
    _pingPongDelay.mix(1.0f);
    _plateReverb.mix(1.0f);

    // Default gains for the output mixers: full dry, no delay or reverb.
    setDryMix(1.0f, 1.0f);
    setDelayMix(0.0f, 0.0f);
    setReverbMix(0.0f, 0.0f);

    // The plate reverb has an internal size and damping; initialize these
    // parameters to the defaults stored in _roomSize and _damping.
    _plateReverb.size(_roomSize);
    // Map damping to both hi and lo damp for a smoother response.
    _plateReverb.hidamp(_damping);
    _plateReverb.lodamp(_damping);
    _plateReverb.bypass_set(false);     // reverb bypass defaults to off, but this is safe


    // Initialize delay feedback to zero.  Without this call the internal
    // delay object defaults to 0.5 feedback; we explicitly set it to 0.
    _pingPongDelay.feedback(0.0f);
    _pingPongDelay.bypass_set(false);   // ensure delay is active
}

// --- Reverb control ---

void FXChainBlock::setReverbRoomSize(float size) {
    // Constrain and store the room size.  Forward the value to the
    // plate reverb.  The plate reverb’s size() method accepts values
    // in the range [0,1] and internally maps them to a reasonable
    // reverb time.
    _roomSize = constrain(size, 0.0f, 1.0f);
    _plateReverb.size(_roomSize);
}

void FXChainBlock::setReverbDamping(float damping) {
    // Constrain and store the damping.  The plate reverb provides
    // separate high‑ and low‑frequency damping controls.  To mimic
    // the single damping parameter exposed by the JT‑4000 UI, apply
    // the same value to both hidamp (treble loss) and lodamp (bass loss).
    _damping = constrain(damping, 0.0f, 1.0f);
    _plateReverb.hidamp(_damping);
    _plateReverb.lodamp(_damping);
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
    // Convert the requested base delay time into a normalized value for the
    // ping‑pong delay.  The effect accepts a 0..1 range which maps
    // logarithmically to 0.._maxDelayMs milliseconds.  Clamp the input
    // between 5 ms and the configured maximum delay range.
    float safeBase = constrain(baseMs, 5.0f, (float)_maxDelayMs);
    float norm = safeBase / (float)_maxDelayMs;
    norm = constrain(norm, 0.0f, 1.0f);
    _pingPongDelay.time(norm);
    // Populate the tap times array for UI display.  Although the new
    // delay effect only has a single delay time, we mimic the original
    // multi‑tap behaviour by computing offsets for each tap.  These
    // values are not used internally by the delay but provide continuity
    // for preset storage and CC feedback.
    for (uint8_t i = 0; i < 8; ++i) {
        float offset = i * safeBase * 0.5f;
        _delayTapTimes[i] = offset;
    }
}

void FXChainBlock::setDelayTapTime(uint8_t tap, float timeMs) {
    // Update the stored tap time.  The new delay effect exposes a single
    // delay time parameter, so any tap adjustment will set the global
    // delay time.  Clamp the requested time to the valid range and
    // convert to a normalized value for the ping‑pong delay.
    if (tap < 8) {
        float safeTime = constrain(timeMs, 5.0f, (float)_maxDelayMs);
        _delayTapTimes[tap] = safeTime;
        float norm = safeTime / (float)_maxDelayMs;
        norm = constrain(norm, 0.0f, 1.0f);
        _pingPongDelay.time(norm);
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
    // Set the dry signal level on mixer input 0 for left and right channels.
    _mixerOutL.gain(0, levelL);
    _mixerOutR.gain(0, levelR);
}

void FXChainBlock::setReverbMix(float levelL, float levelR) {
    _reverbMixL = levelL;
    _reverbMixR = levelR;
    // Set the reverb wet level on mixer input 2.  This controls the
    // amount of plate reverb in the final mix.  Channels 2 on
    // _mixerOutL/R correspond to the reverb output.
    _mixerOutL.gain(2, levelL);
    _mixerOutR.gain(2, levelR);
}

void FXChainBlock::setDelayMix(float levelL, float levelR) {
    _delayMixL = levelL;
    _delayMixR = levelR;
    // Set the delay wet level on mixer input 1.  This controls how
    // much of the ping‑pong delay output (pre‑reverb) is present in
    // the final mix.  Channels 1 on _mixerOutL/R correspond to the
    // delay output.
    _mixerOutL.gain(1, levelL);
    _mixerOutR.gain(1, levelR);
}

void FXChainBlock::setDelayFeedback(float delayFeedback) {
    // Constrain the feedback parameter to 0..1 and store it.  Pass it
    // directly to the ping‑pong delay.  Values near 1.0 result in
    // longer repeats but may cause runaway feedback; use with care.
    _delayFeedback = constrain(delayFeedback, 0.0f, 1.0f);
    _pingPongDelay.feedback(_delayFeedback);
}

// --- Access outputs ---

AudioMixer4& FXChainBlock::getOutputLeft() {
    return _mixerOutL;
}

AudioMixer4& FXChainBlock::getOutputRight() {
    return _mixerOutR;
}
