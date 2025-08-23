#pragma once
#include <Arduino.h>
#include "SynthEngine.h"

// Nine basic templates: OSC1 full mix, tuning mid, filter open no res,
// drive=1, gain=1, ENV: A=0 D=0 S=1 R=0, no FX, no LFO.
namespace Presets {

  // Load the “init” template for a given OSC1 waveform index (0..8).
  // Applies by sending CC values through SynthEngine::handleControlChange
  // to keep everything aligned with your current CC pipeline.
  void loadInitTemplateByWave(SynthEngine& synth, uint8_t waveIndex);

  // Convenience: load by a simple 0..8 number (same as waveIndex).
  inline void loadTemplateByIndex(SynthEngine& synth, uint8_t idx) {
    loadInitTemplateByWave(synth, idx);
  }

  // Optional: get a friendly name for UI/debug (“Init Saw”, etc.)
  const char* templateName(uint8_t idx);
}
