/*
 * FXChainBlock.h
 *
 * Revised FX chain for the JT‑4000.  This version replaces the old multi‑tap
 * delay and Freeverb with the stereo modulated ping‑pong delay and plate
 * reverb from the hexefx_audiolib_i16 library.  Audio flows in parallel:
 * the amplifier feeds directly into the delay and reverb, and the wet
 * outputs of each effect are mixed alongside the dry signal.  When an
 * effect’s mix level is zero its internal processing is bypassed to save
 * CPU time.  Additional setters expose modulation rate, modulation depth,
 * inertia and tone controls for the delay, and separate high/low damping
 * controls for the reverb.
 */

#pragma once

#include <Audio.h>

// Pull in the hexefx effect headers.  These must be installed in the
// Arduino/library search path (e.g. lib/hexefx_audiolib_i16/src).  If you
// encounter build errors, verify the include paths match your project.
#include "effect_delaystereo_i16.h"
#include "effect_platereverb_i16.h"

class FXChainBlock {
public:
    FXChainBlock();

    // --- Reverb controls ---
    void setReverbRoomSize(float size);            // 0..1
    void setReverbHiDamping(float damping);        // 0..1 (high frequencies)
    void setReverbLoDamping(float damping);        // 0..1 (low frequencies)
    float getReverbRoomSize() const;
    float getReverbHiDamping() const;
    float getReverbLoDamping() const;

    // --- Delay controls ---
    void setDelayTimeMs(float ms);                 // absolute time in ms, clipped
    float getDelayTimeMs() const;
    void setDelayFeedback(float feedback);         // 0..1
    float getDelayFeedback() const;
    void setDelayModRate(float rate);              // 0..1
    void setDelayModDepth(float depth);            // 0..1
    void setDelayInertia(float inertia);           // 0..1
    void setDelayTreble(float treble);             // 0..1
    void setDelayBass(float bass);                 // 0..1
    float getDelayModRate() const;
    float getDelayModDepth() const;
    float getDelayInertia() const;
    float getDelayTreble() const;
    float getDelayBass() const;

    // --- Mix controls ---
    void setDryMix(float levelL, float levelR);    // 0..1 per channel
    void setReverbMix(float levelL, float levelR); // 0..1 per channel
    void setDelayMix(float levelL, float levelR);  // 0..1 per channel
    float getDryMixL() const { return _dryMixL; }
    float getDryMixR() const { return _dryMixR; }
    float getReverbMixL() const { return _reverbMixL; }
    float getReverbMixR() const { return _reverbMixR; }
    float getDelayMixL() const { return _delayMixL; }
    float getDelayMixR() const { return _delayMixR; }

    // Outputs for patching into the synth engine
    AudioMixer4& getOutputLeft();
    AudioMixer4& getOutputRight();

    // Amplifier input for the summed voices
    AudioAmplifier _ampIn;

private:
    // Maximum delay time in milliseconds.  Increase if you need longer echoes.
    static constexpr float _maxDelayMs = 2000.0f;

    // Audio effects
    AudioEffectDelayStereo_i16 _pingPongDelay{2000, true};
    AudioEffectPlateReverb_i16 _plateReverb;

    // Final mix
    AudioMixer4 _mixerOutL;
    AudioMixer4 _mixerOutR;

    // Patch cords (allocated in ctor)
    AudioConnection* _patchAmpToDelay;
    AudioConnection* _patchAmpToReverb;
    AudioConnection* _patchAmpToMixL;
    AudioConnection* _patchAmpToMixR;
    AudioConnection* _patchDelayToMixL;
    AudioConnection* _patchDelayToMixR;
    AudioConnection* _patchReverbToMixL;
    AudioConnection* _patchReverbToMixR;

    // Internal state for UI and CC access
    float _roomSize = 0.5f;
    float _reverbHiDamp = 0.5f;
    float _reverbLoDamp = 0.5f;
    float _delayTimeMs  = 400.0f;
    float _delayFeedback = 0.5f;
    float _delayModRate = 0.0f;
    float _delayModDepth = 0.0f;
    float _delayInertia = 0.0f;
    float _delayTreble = 0.5f;
    float _delayBass = 0.5f;
    float _dryMixL = 1.0f, _dryMixR = 1.0f;
    float _reverbMixL = 0.0f, _reverbMixR = 0.0f;
    float _delayMixL = 0.0f, _delayMixR = 0.0f;
};
