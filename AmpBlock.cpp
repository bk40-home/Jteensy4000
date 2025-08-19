#include "AmpBlock.h"

// --- Lifecycle
void AmpBlock::update() {
    // Placeholder for updating amp parameters (e.g., gain modulation)
}


// --- Outputs
AudioStream& AmpBlock::output() {
    return _amp;
}

void AmpBlock::setGain(float gain) {
    _amp.gain(gain);
}


AudioAmplifier* AmpBlock::getAmplifier() {
    return &_amp;
}
