#include "LFOBlock.h"


// --- Lifecycle
LFOBlock::LFOBlock() {
    // Initialise the waveform generator but leave it muted.  By default the
    // LFO is disabled until a destination and amplitude are set.  This
    // prevents unnecessary CPU usage when no modulation is required.
    _lfo.begin(_type);
    _lfo.amplitude(0.0f);
    _lfo.frequency(5);
    _lfo.pulseWidth(0.5);
    _enabled = false;
}

// ADD new method implementation:
void LFOBlock::setTimingMode(TimingMode mode) {
    _timingMode = mode;
    
    if (mode == TIMING_FREE) {
        // Restore free-running frequency
        setFrequency(_freeRunningFreq);
    }
    // When switching to BPM mode, frequency will be updated by
    // updateFromBPMClock() call from SynthEngine
}

void LFOBlock::updateFromBPMClock(const BPMClockManager& bpmClock) {
    // Only update if in BPM-synced mode
    if (_timingMode == TIMING_FREE) return;
    
    // Get frequency for current timing mode
    float syncedFreq = bpmClock.getFrequencyForMode(_timingMode);
    if (syncedFreq > 0.0f) {
        _lfo.frequency(syncedFreq);  // Update LFO directly, bypass cached _freq
    }
}

// MODIFY setFrequency (line 40):
void LFOBlock::setFrequency(float hz) {
    _freeRunningFreq = hz;  // Always store for mode switching
    
    // Only apply if in free-running mode
    if (_timingMode == TIMING_FREE) {
        _freq = hz;
        _lfo.frequency(hz);
        Serial.printf("LFO setFrequency %.3f Hz (FREE)\n", _freq);
    } else {
        Serial.printf("LFO in sync mode, freq managed by BPM clock\n");
    }
}

void LFOBlock::update() {    
    // Keep the LFO free‑running only when enabled.  If disabled, we
    // simply leave the internal oscillator muted.  We intentionally do
    // not stop the phase accumulator so that re‑enabling later will
    // pick up where it left off, preserving free‑running behaviour.
    if (!_enabled) {
        // Ensure the amplitude stays at zero while disabled
        _lfo.amplitude(0.0f);
        return;
    }
    // When enabled, ensure the amplitude reflects the user’s setting
    _lfo.amplitude(_amp);
}

void LFOBlock::setWaveformType(int type) {
    _type = type;
    _lfo.begin(_type);
        // If user selected a pulse-type LFO, ensure a valid width
    if (_type == WAVEFORM_PULSE || _type == WAVEFORM_BANDLIMIT_PULSE) {
        _lfo.pulseWidth(0.5f);
    }
}



void LFOBlock::setDestination(LFODestination destination) {
    _destination = destination;
    // Automatically enable or disable based on destination.  If no
    // destination is selected, there’s no need to run the LFO.
    if (_destination == LFO_DEST_NONE) {
        setEnabled(false);
    } else {
        // Only enable if the amplitude is non‑zero
        setEnabled(_amp > 0.0f);
    }
}

void LFOBlock::setAmplitude(float amp) {
    _amp = amp;
    // If enabled, apply the new amplitude.  Otherwise, keep the
    // underlying oscillator muted.  We still store the requested
    // amplitude for later.
    if (_enabled) {
        _lfo.amplitude(_amp);
    } else {
        _lfo.amplitude(0.0f);
    }
    Serial.printf("setAmplitude %.3f\n",  _amp);
    // If the amplitude becomes zero, disable the LFO to save CPU
    if (_amp <= 0.0f) {
        setEnabled(false);
    } else {
        // If a valid destination is set, re‑enable the LFO
        if (_destination != LFO_DEST_NONE) {
            setEnabled(true);
        }
    }
}

// --- Parameter Getters
float LFOBlock::getFrequency() const {
    return _freq;
}

int LFOBlock::getWaveform() const {
    return _type;
}

LFODestination LFOBlock::getDestination() const {
    return _destination;
}
float LFOBlock::getAmplitude() const {
    return _amp;
}

AudioStream& LFOBlock::output(){ 
    return _lfo;
}

// === Enabled State ===
void LFOBlock::setEnabled(bool enabled) {
    _enabled = enabled;
    if (_enabled) {
        // Restore the user’s amplitude when enabling
        _lfo.amplitude(_amp);
    } else {
        // Mute the waveform when disabled
        _lfo.amplitude(0.0f);
    }
}

bool LFOBlock::isEnabled() const {
    return _enabled;
}


