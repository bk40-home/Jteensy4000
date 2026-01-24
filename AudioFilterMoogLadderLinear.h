#pragma once
#include <Arduino.h>
#include "AudioStream.h"

// Linear Moog ladder: 4x identical ZDF one-poles in cascade with feedback.
// Now exposes cutoff/resonance modulators as extra inputs, Audio.h-style.

class AudioFilterMoogLadderLinear : public AudioStream {
public:
  // NOTE: now 3 inputs to support mod buses
  AudioFilterMoogLadderLinear() : AudioStream(3, _inQ) {}

  void frequency(float hz) { _fcTarget = constrain(hz, 5.0f, AUDIO_SAMPLE_RATE_EXACT*0.45f); }
  void resonance(float k)  { _k = max(0.0f, k); }          // self-osc ~ k≈4
  void portamento(float ms){ _portaMs = max(0.0f, ms); }

  // NEW: modulation scaling (same as diode version)
  void setCutoffModOctaves(float oct) { _modOct = max(0.0f, oct); }
  void setResonanceModDepth(float d)  { _resModDepth = max(0.0f, d); }

  virtual void update(void) override;

private:
  audio_block_t* _inQ[3];

  // TPT states
  float _s1=0,_s2=0,_s3=0,_s4=0;

  // last outputs
  float _y1=0,_y2=0,_y3=0,_y4=0;

  // control
  float _fs       = AUDIO_SAMPLE_RATE_EXACT;
  float _fc       = 1000.0f;
  float _fcTarget = 1000.0f;
  float _k        = 0.0f;
  float _portaMs  = 0.0f;

  // feedback guards
  float _dc  = 0.0f;     // DC tracker
  float _env = 0.0f;     // envelope for thresholded safe-k

  // Mod scaling
  float _modOct      = 0.0f; // octaves per +1 on In1
  float _resModDepth = 0.0f; // k units per +1 on In2

  inline float cutoffAlpha() const {
    if (_portaMs <= 0.0f) return 1.0f;
    const float tau = _portaMs * 0.001f;
    return 1.0f - expf(-1.0f / (tau * _fs));
  }
};

