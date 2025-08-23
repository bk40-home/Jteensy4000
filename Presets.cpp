#include "Presets.h"
#include "CCMap.h"   // for CC numbers consistency if needed

namespace {

// Helper to send one CC value through the existing engine path
inline void sendCC(SynthEngine& synth, uint8_t cc, uint8_t val, uint8_t ch = 1) {
  synth.handleControlChange(ch, cc, val);
}

} // anon

namespace Presets {

const char* templateName(uint8_t idx) {
  static const char* names[9] = {
    "Init Wave 0","Init Wave 1","Init Wave 2","Init Wave 3","Init Wave 4",
    "Init Wave 5","Init Wave 6","Init Wave 7","Init Wave 8"
  };
  return (idx < 9) ? names[idx] : "Init";
}

void loadInitTemplateByWave(SynthEngine& synth, uint8_t waveIndex) {
  if (waveIndex > 8) waveIndex = 0;

  // We’ll apply atomically to reduce zippering/FX artifacts.
  AudioNoInterrupts();

  // ---------------- Core oscillator setup ----------------
  // Waveforms
  sendCC(synth, 21, waveIndex);    // OSC1 Waveform (0..8)
  sendCC(synth, 22, 0);            // OSC2 Waveform (don’t care, OSC2 is muted)

  // Mixes (explicit taps to avoid inverse-pair surprises)
  sendCC(synth, 60, 127);          // OSC1 Mix = 1.0
  sendCC(synth, 61, 0);            // OSC2 Mix = 0.0
  sendCC(synth, 58, 0);            // Sub Mix  = 0.0
  sendCC(synth, 59, 0);            // Noise Mix= 0.0

  // Tuning midpoints
  sendCC(synth, 41, 64);           // OSC1 Pitch bucket @ 0 semis
  sendCC(synth, 45, 64);           // OSC1 Fine @ 0 cents
  sendCC(synth, 43, 64);           // OSC1 Detune center (-1..1 mapped to 0..127)

  // (OSC2 left silent; set to neutral anyway)
  sendCC(synth, 42, 64);
  sendCC(synth, 46, 64);
  sendCC(synth, 44, 64);

  // ---------------- Filter: open, no res ----------------
  sendCC(synth, 23, 127);          // Cutoff wide open
  sendCC(synth, 24, 0);            // Resonance 0
  sendCC(synth, 48, 0);            // Filter Env Amount 0
  sendCC(synth, 50, 0);            // KeyTrack 0 (adjust if you prefer 50%/100%)

  // Filter extras: Drive=1, Passband Gain=1 (per your request)
  // CC83: Drive 0..5 → 0..127; Drive=1 → ~ 1/5 → ~25
  sendCC(synth, 83, 25);
  // CC85: Passband Gain 0..3 → 0..127; Gain=1 → ~1/3 → ~42
  sendCC(synth, 85, 42);
  // CC84: Octave Control leave at 0
  sendCC(synth, 84, 0);

  // ---------------- Envelopes: A=0, D=0, S=1, R=0 --------------
  // Amp Env
  sendCC(synth, 25, 0);            // A
  sendCC(synth, 26, 0);            // D
  sendCC(synth, 27, 127);          // S = 1.0
  sendCC(synth, 28, 0);            // R

  // Filter Env (same)
  sendCC(synth, 29, 0);
  sendCC(synth, 30, 0);
  sendCC(synth, 31, 127);
  sendCC(synth, 32, 0);

  // ---------------- LFOs: off --------------------------
  sendCC(synth, 55, 0);            // LFO1 Amount
  sendCC(synth, 52, 0);            // LFO2 Amount
  // (Rates/dests left alone—they won’t matter at zero amount)

  // ---------------- FX: none ---------------------------
  // Page 13: 70=RevSize, 71=RevDamp, 72=DelayTime, 73=Feedback
  sendCC(synth, 70, 0);
  sendCC(synth, 71, 0);
  sendCC(synth, 72, 0);
  sendCC(synth, 73, 0);

  // Page 14: 74=Dry, 75=Reverb, 76=Delay (mixes)
  sendCC(synth, 74, 127);          // Full dry
  sendCC(synth, 75, 0);            // No reverb
  sendCC(synth, 76, 0);            // No delay

  // ---------------- Glide / AmpMod ----------------------
  sendCC(synth, 81, 0);            // Glide disabled
  sendCC(synth, 82, 0);            // Glide time 0
  sendCC(synth, 90, 0);            // Amp Mod DC = 0

  AudioInterrupts();
}

} // namespace Presets
