#include "FilterBlock.h"

FilterBlock::FilterBlock() {
    _patchCables[0] = new AudioConnection(_modMixer, 0, _filter, 1);
    //_patchCables[1] = new AudioConnection(_envModDc, 0, _modMixer, 0);
    _patchCables[1] = new AudioConnection(_keyTrackDc, 0, _modMixer,0);

    _envModDc.amplitude(0.0f);
    _keyTrackDc.amplitude(0.5f);

    _modMixer.gain(0, 1.0f);
    _modMixer.gain(1, 1.0f);
    _modMixer.gain(2, 0.0f);
    _modMixer.gain(3, 0.0f);

    _filter.octaveControl(_octaveControl);
    _filter.passbandGain(_passbandGain);
}

void FilterBlock::setCutoff(float freqHz) {
    //freqHz = constrain(freqHz, 0.0f, 16500.0f);  // Allow full audio-range cutoff
    if (freqHz != _cutoff) {
        _cutoff = freqHz;
        _filter.frequency(freqHz);
        Serial.printf("[FilterBlock] Set Cutoff: %.2f Hz\n", freqHz);
    }
}

void FilterBlock::setResonance(float amount) {
    //_resonance = constrain(amount, 0.0f, 1.5f);
    _filter.resonance(amount);
    // Serial.printf("[FilterBlock] Set Resonance: %.2f\n", amount);
}

void FilterBlock::setDrive(float amount) {
    _drive = amount;
    _filter.inputDrive(_drive);
    // Serial.printf("[FilterBlock] Set Drive: %.2f\n", _drive);
}

void FilterBlock::setOctaveControl(float octaves) {
    _octaveControl = octaves;
    _filter.octaveControl(octaves);
    // Serial.printf("[FilterBlock] Set Octave Control: %.2f\n", octaves);
}

void FilterBlock::setPassbandGain(float gain) {
    _passbandGain = gain;
    _filter.passbandGain(gain);
    // Serial.printf("[FilterBlock] Set Passband Gain: %.2f\n", gain);
}

void FilterBlock::setEnvModAmount(float amount) {
    _envModAmount = amount;
    _envModDc.amplitude(amount);
     Serial.printf("[FilterBlock] Env Mod Amount: %.2f\n", amount);
}


void FilterBlock::setKeyTrackAmount(float amount) {
    _keyTrackAmount = amount;
    _keyTrackDc.amplitude(amount);
     Serial.printf("[FilterBlock] Key Track Amount: %.2f\n", amount);
}


float FilterBlock::getCutoff() const { return _cutoff; }
float FilterBlock::getResonance() const { return _resonance; }
float FilterBlock::getDrive() const { return _drive; }
float FilterBlock::getOctaveControl() const { return _octaveControl; }
float FilterBlock::getPassbandGain() const { return _passbandGain; }
float FilterBlock::getEnvModAmount() const { return _envModAmount; }
float FilterBlock::getKeyTrackAmount() const { return _keyTrackAmount; }

AudioStream& FilterBlock::input() { return _filter; }
AudioStream& FilterBlock::output() { return _filter; }
AudioStream& FilterBlock::envmod() { return _envModDc; };
AudioMixer4& FilterBlock::modMixer() { return _modMixer; }
