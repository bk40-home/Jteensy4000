#include "LFOBlock.h"


// --- Lifecycle
LFOBlock::LFOBlock() {
    _lfo.begin(_type);
    _lfo.amplitude(_amp);
    _lfo.frequency(5);
    _lfo.pulseWidth(0.5);
}

void LFOBlock::update() {    
}

void LFOBlock::setWaveformType(int type) {
    _type = type;
    _lfo.begin(_type);
}

// --- Parameter Setters
void LFOBlock::setFrequency(float hz) {
    _freq = hz;
    _lfo.frequency(hz);
    Serial.printf("setFrequency %.3f\n",  _freq);
}

void LFOBlock::setDestination(LFODestination destination) {
    _destination = destination;

}

void LFOBlock::setAmplitude(float amp) {
    _amp = amp;
    _lfo.amplitude(_amp);
    Serial.printf("setAmplitude %.3f\n",  _amp);
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


