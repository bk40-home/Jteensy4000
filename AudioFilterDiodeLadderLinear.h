#pragma once
#include <Arduino.h>
#include "AudioStream.h"

/* ============================================================================
 *  AudioFilterDiodeLadderLinear
 *  -----------------------------
 *  Linear diode ladder (Zavalishin) in ZDF/TPT form for Teensy Audio.
 *  - Four TPT 1-pole stages with diode-ladder couplings:
 *      u1 = x_fb + y2
 *      u2 = 0.5*(y1 + y3)
 *      u3 = 0.5*(y2 + y4)
 *      u4 = 0.5*y3
 *  - Stage 1 uses 2*g, stages 2..4 use g (critical!).
 *  - Feedback: x_fb = x - k*y4  (k ~ 0..17 hits linear self-osc)
 *  - Cutoff glide via simple 1-pole smoothing (portamento on cutoff).
 *
 *  API mirrors your ladder style: frequency(), resonance(), inputDrive(ignore),
 *  portamento(). Fully commented; no deletions to your existing code.
 * ========================================================================== */
class AudioFilterDiodeLadderLinear : public AudioStream {
public:
  AudioFilterDiodeLadderLinear() : AudioStream(1, _inQueue) {}

  // Set cutoff target (Hz). Smoothed per-sample by portamento().
  void frequency(float hz) { _fcTarget = constrain(hz, 5.0f, AUDIO_SAMPLE_RATE_EXACT * 0.45f); }

  // Set resonance gain (k). Linear self-osc ~ 17; we cap a bit lower by default.
  void resonance(float k)  { _k = constrain(k, 0.0f, _kMax); }

  // Parity with other filters (ignored in linear model).
  void inputDrive(float) {}

  // Cutoff glide in milliseconds (0 = off).
  void portamento(float ms) { _portaMs = max(0.0f, ms); }

  // Optional: adjust safe cap for UI
  void setResonanceMax(float kmax) { _kMax = max(0.0f, kmax); }

  virtual void update(void) override;

private:
  audio_block_t* _inQueue[1];

  // TPT states for 4 stages
  float _s1 = 0, _s2 = 0, _s3 = 0, _s4 = 0;

  // Keep last outputs as GS initial guess
  float _y1 = 0, _y2 = 0, _y3 = 0, _y4 = 0;

  // Control
  float _fs       = AUDIO_SAMPLE_RATE_EXACT;
  float _fc       = 1000.0f;
  float _fcTarget = 1000.0f;
  float _k        = 0.0f;
  float _kMax     = 16.0f;     // safe default for linear
  float _portaMs  = 0.0f;


float _dc = 0.0f;      // DC tracker for feedback HPF
float _env = 0.0f;     // envelope follower for safe-k
float _y4_last = 0.0f; // last output sample (for feedback processing)

float _clampState = 0.0f; // 0=off, 1=on-ish (smoothed)


  inline float cutoffAlpha() const {
    if (_portaMs <= 0.0f) return 1.0f;
    const float tau = _portaMs * 0.001f;
    return 1.0f - expf(-1.0f / (tau * _fs));
  }
};
