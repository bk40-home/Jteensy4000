#pragma once
#include <Audio.h>

// AmpBlock processes audio amplitude with modulation support
class AmpBlock {
public:
    // --- Lifecycle
    void update();

    void setGain(float gain);

    AudioStream& output();

    AudioAmplifier* getAmplifier();


    // --- Outputs
    audio_block_t* getOutputPointer();

private:
    AudioAmplifier _amp;
};