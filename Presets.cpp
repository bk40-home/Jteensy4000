#include "Presets.h"
#include "CCMap.h"   // for CC numbers consistency if needed
#include "Mapping.h"
#include "Presets_Microsphere.h" // already in your tree

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
// --- put near the top of Presets.cpp ---
static const int kTEMPLATE_COUNT = 9; // you expose 9: "Init Wave 0" .. "Init Wave 8"

int presets_templateCount() { return kTEMPLATE_COUNT; }
int presets_totalCount()    { return kTEMPLATE_COUNT + JT4000_PRESET_COUNT; }

const char* presets_nameByGlobalIndex(int idx) {
  if (idx < 0) return "—";
  if (idx < kTEMPLATE_COUNT) return templateName((uint8_t)idx);
  int bankIdx = idx - kTEMPLATE_COUNT;
  if (bankIdx >= 0 && bankIdx < JT4000_PRESET_COUNT) return JT4000_Presets[bankIdx].name;
  return "—";
}

void presets_loadByGlobalIndex(SynthEngine& synth, int globalIdx, uint8_t midiCh) {
  const int total = presets_totalCount();
  if (total <= 0) return;
  while (globalIdx < 0) globalIdx += total;
  while (globalIdx >= total) globalIdx -= total;

  if (globalIdx < kTEMPLATE_COUNT) {
    loadInitTemplateByWave(synth, (uint8_t)globalIdx);
  } else {
    const int bankIdx = globalIdx - kTEMPLATE_COUNT;
    loadMicrospherePreset(synth, bankIdx, midiCh);
  }
}

void loadInitTemplateByWave(SynthEngine& synth, uint8_t waveIndex) {
  if (waveIndex > 8) waveIndex = 0;

  // We’ll apply atomically to reduce zippering/FX artifacts.
  AudioNoInterrupts();

  // ---------------- Core oscillator setup ----------------
  // Waveforms
  sendCC(synth, 21, waveIndex);    // OSC1 Waveform (0..8)
  sendCC(synth, 22, waveIndex);            // OSC2 Waveform (don’t care, OSC2 is muted)

  // Mixes (explicit taps to avoid inverse-pair surprises)
  sendCC(synth, 60, 127);          // OSC1 Mix = 1.0
  sendCC(synth, 61, 127);            // OSC2 Mix = 0.0
  sendCC(synth, 58, 0);            // Sub Mix  = 0.0
  sendCC(synth, 59, 0);            // Noise Mix= 0.0

  // Tuning midpoints
  sendCC(synth, 41, 65);           // OSC1 Pitch bucket @ 0 semis
  sendCC(synth, 45, 64);           // OSC1 Fine @ 0 cents
  sendCC(synth, 43, 65);           // OSC1 Detune center (-1..1 mapped to 0..127)

  // (OSC2 left silent; set to neutral anyway)
  sendCC(synth, 42, 65);
  sendCC(synth, 46, 66);
  sendCC(synth, 44, 65);

  // ---------------- Filter: open, no res ----------------
  sendCC(synth, 23, 127);          // Cutoff wide open
  sendCC(synth, 24, 0);            // Resonance 0
  sendCC(synth, 48, 65);            // Filter Env Amount 0
  sendCC(synth, 50, 65);            // KeyTrack 0 (adjust if you prefer 50%/100%)

  // Filter extras: Drive=1, Passband Gain=1 (per your request)
  // CC83: Drive 0..5 → 0..127; Drive=1 → ~ 1/5 → ~25
  //sendCC(synth, 83, 65);
  // CC85: Passband Gain 0..3 → 0..127; Gain=1 → ~1/3 → ~42
  //sendCC(synth, 85, 64);
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
  sendCC(synth, 90, 127);            // Amp Mod DC = 0

  AudioInterrupts();
}

void loadRawPatchViaCC(SynthEngine& synth, const uint8_t data[64], uint8_t midiCh) {
  AudioNoInterrupts();
  for (const auto& row : JT4000Map::kSlots) {
    const uint8_t idx0 = (row.byte1 >= 1) ? (row.byte1 - 1) : 0;
    if (idx0 >= 64) continue;
    const uint8_t raw = data[idx0];
    const uint8_t val = JT4000Map::toCC(raw, row.xf);
    sendCC(synth, row.cc, val, midiCh);

  }
  AudioInterrupts();
}

void loadMicrospherePreset(SynthEngine& synth, int index, uint8_t midiCh) {
  if (index < 0 || index >= JT4000_PRESET_COUNT) return;
  loadRawPatchViaCC(synth, JT4000_Presets[index].data, midiCh);
}

} // namespace Presets
