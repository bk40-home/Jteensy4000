/*
 * FXChainBlock.cpp
 *
 * Implements the revised FX chain using the stereo modulated ping‑pong delay
 * and plate reverb from hexefx_audiolib_i16.  See FXChainBlock.h for
 * architecture notes.  Audio routing is: amplifier → delay, amplifier →
 * reverb, amplifier → mixer dry.  The wet outputs of the delay and reverb
 * are then mixed with the dry signal using AudioMixer4.  When an effect’s
 * mix is set to zero it is bypassed internally to reduce CPU usage.
 */

#include "FXChainBlock.h"

FXChainBlock::FXChainBlock()
    : _pingPongDelay(_maxDelayMs, /*use_psram=*/true),
      _plateReverb()
{
    // Initialise the delay effect.  The constructor already allocates
    // memory for the maximum delay time.  Call begin() anyway to make it
    // explicit and to support dynamic reallocation if needed.
    //_pingPongDelay.begin((uint32_t)_maxDelayMs, /*use_psram=*/true);
    _pingPongDelay.bypass_set(false);
    _pingPongDelay.mix(1.0f);               // fully wet; dry handled by mixer
    _pingPongDelay.feedback(_delayFeedback);
    _pingPongDelay.time(_delayTimeMs / _maxDelayMs);
    _pingPongDelay.mod_rate(_delayModRate);
    _pingPongDelay.mod_depth(_delayModDepth);
    _pingPongDelay.inertia(_delayInertia);
    _pingPongDelay.treble(_delayTreble);
    _pingPongDelay.bass(_delayBass);

    // Initialise the reverb effect
    _plateReverb.bypass_set(false);
    _plateReverb.mix(1.0f);               // fully wet; dry handled by mixer
    _plateReverb.size(_roomSize);
    _plateReverb.hidamp(_reverbHiDamp);
    _plateReverb.lodamp(_reverbLoDamp);

    // Create patch cords for routing.  Each connection is allocated on the
    // heap to match the original design (can be freed in destructor if needed).
    _patchAmpToDelay   = new AudioConnection(_ampIn, _pingPongDelay);
    _patchAmpToReverb  = new AudioConnection(_ampIn, _plateReverb);
    _patchAmpToMixL    = new AudioConnection(_ampIn, 0, _mixerOutL, 0);
    _patchAmpToMixR    = new AudioConnection(_ampIn, 0, _mixerOutR, 0);
    _patchDelayToMixL  = new AudioConnection(_pingPongDelay, 0, _mixerOutL, 1);
    _patchDelayToMixR  = new AudioConnection(_pingPongDelay, 0, _mixerOutR, 1);
    _patchReverbToMixL = new AudioConnection(_plateReverb, 0, _mixerOutL, 2);
    _patchReverbToMixR = new AudioConnection(_plateReverb, 0, _mixerOutR, 2);

    // Default mixer gains: dry = 1, delay = 0, reverb = 0
    setDryMix(_dryMixL, _dryMixR);
    setDelayMix(_delayMixL, _delayMixR);
    setReverbMix(_reverbMixL, _reverbMixR);
}

// -----------------------------------------------------------------------------
// Reverb parameter setters/getters
// -----------------------------------------------------------------------------
void FXChainBlock::setReverbRoomSize(float size) {
    // Constrain and store
    if (size < 0.0f) size = 0.0f;
    if (size > 1.0f) size = 1.0f;
    _roomSize = size;
    _plateReverb.size(size);
}

void FXChainBlock::setReverbHiDamping(float damping) {
    if (damping < 0.0f) damping = 0.0f;
    if (damping > 1.0f) damping = 1.0f;
    _reverbHiDamp = damping;
    _plateReverb.hidamp(damping);
}

void FXChainBlock::setReverbLoDamping(float damping) {
    if (damping < 0.0f) damping = 0.0f;
    if (damping > 1.0f) damping = 1.0f;
    _reverbLoDamp = damping;
    _plateReverb.lodamp(damping);
}

float FXChainBlock::getReverbRoomSize() const { return _roomSize; }
float FXChainBlock::getReverbHiDamping() const { return _reverbHiDamp; }
float FXChainBlock::getReverbLoDamping() const { return _reverbLoDamp; }

// -----------------------------------------------------------------------------
// Delay parameter setters/getters
// -----------------------------------------------------------------------------
void FXChainBlock::setDelayTimeMs(float ms) {
    // Enforce safe limits: 5.._maxDelayMs
    if (ms < 5.0f) ms = 5.0f;
    if (ms > _maxDelayMs) ms = _maxDelayMs;
    _delayTimeMs = ms;
    float norm = ms / _maxDelayMs;
    _pingPongDelay.time(norm);
}

float FXChainBlock::getDelayTimeMs() const { return _delayTimeMs; }

void FXChainBlock::setDelayFeedback(float feedback) {
    if (feedback < 0.0f) feedback = 0.0f;
    if (feedback > 1.0f) feedback = 1.0f;
    _delayFeedback = feedback;
    _pingPongDelay.feedback(feedback);
}

float FXChainBlock::getDelayFeedback() const { return _delayFeedback; }

void FXChainBlock::setDelayModRate(float rate) {
    if (rate < 0.0f) rate = 0.0f;
    if (rate > 1.0f) rate = 1.0f;
    _delayModRate = rate;
    _pingPongDelay.mod_rate(rate);
}

void FXChainBlock::setDelayModDepth(float depth) {
    if (depth < 0.0f) depth = 0.0f;
    if (depth > 1.0f) depth = 1.0f;
    _delayModDepth = depth;
    _pingPongDelay.mod_depth(depth);
}

void FXChainBlock::setDelayInertia(float inertia) {
    if (inertia < 0.0f) inertia = 0.0f;
    if (inertia > 1.0f) inertia = 1.0f;
    _delayInertia = inertia;
    _pingPongDelay.inertia(inertia);
}

void FXChainBlock::setDelayTreble(float treble) {
    if (treble < 0.0f) treble = 0.0f;
    if (treble > 1.0f) treble = 1.0f;
    _delayTreble = treble;
    _pingPongDelay.treble(treble);
}

void FXChainBlock::setDelayBass(float bass) {
    if (bass < 0.0f) bass = 0.0f;
    if (bass > 1.0f) bass = 1.0f;
    _delayBass = bass;
    _pingPongDelay.bass(bass);
}

float FXChainBlock::getDelayModRate() const { return _delayModRate; }
float FXChainBlock::getDelayModDepth() const { return _delayModDepth; }
float FXChainBlock::getDelayInertia() const { return _delayInertia; }
float FXChainBlock::getDelayTreble() const { return _delayTreble; }
float FXChainBlock::getDelayBass() const { return _delayBass; }

// -----------------------------------------------------------------------------
// Mix control
// -----------------------------------------------------------------------------
void FXChainBlock::setDryMix(float levelL, float levelR) {
    if (levelL < 0.0f) levelL = 0.0f; if (levelL > 1.0f) levelL = 1.0f;
    if (levelR < 0.0f) levelR = 0.0f; if (levelR > 1.0f) levelR = 1.0f;
    _dryMixL = levelL;
    _dryMixR = levelR;
    _mixerOutL.gain(0, levelL);
    _mixerOutR.gain(0, levelR);
}

void FXChainBlock::setReverbMix(float levelL, float levelR) {
    if (levelL < 0.0f) levelL = 0.0f; if (levelL > 1.0f) levelL = 1.0f;
    if (levelR < 0.0f) levelR = 0.0f; if (levelR > 1.0f) levelR = 1.0f;
    _reverbMixL = levelL;
    _reverbMixR = levelR;
    _mixerOutL.gain(2, levelL);
    _mixerOutR.gain(2, levelR);
    // Bypass reverb when silent
    bool bypass = (levelL <= 0.0f && levelR <= 0.0f);
    _plateReverb.bypass_set(bypass);
}

void FXChainBlock::setDelayMix(float levelL, float levelR) {
    if (levelL < 0.0f) levelL = 0.0f; if (levelL > 1.0f) levelL = 1.0f;
    if (levelR < 0.0f) levelR = 0.0f; if (levelR > 1.0f) levelR = 1.0f;
    _delayMixL = levelL;
    _delayMixR = levelR;
    _mixerOutL.gain(1, levelL);
    _mixerOutR.gain(1, levelR);
    // Bypass delay when silent
    bool bypass = (levelL <= 0.0f && levelR <= 0.0f);
    _pingPongDelay.bypass_set(bypass);
}

// -----------------------------------------------------------------------------
// Output access
// -----------------------------------------------------------------------------
AudioMixer4& FXChainBlock::getOutputLeft()  { return _mixerOutL; }
AudioMixer4& FXChainBlock::getOutputRight() { return _mixerOutR; }