/**
 * Jteensy4000.ino — JT-4000 polyphonic synthesizer main sketch
 *
 * Audio path:
 *   SynthEngine (8 voices) -> FXChainBlock -> AudioMixer4 (I2S split)
 *                                           -> AudioAmplifier  (USB split)
 *   I2S output: AudioOutputI2S2 -> PCM5102A DAC
 *   USB output: AudioOutputUSB  (for DAW monitoring)
 *
 * PCM5102A DAC notes:
 *   XSMT (mute) pin MUST be driven HIGH after I2S starts or DAC stays silent.
 *   FMT  pin = LOW  -> I2S format (default)
 *   SCK  pin = LOW  -> no master clock needed (I2S2 supplies BCK/LRCK)
 *   Pin used: DAC_MUTE_PIN (34) — wire to PCM5102A XSMT.
 *
 * Encoder notes:
 *   Pins 28-32 must NOT use attachInterrupt() on Teensy 4.1.
 *   GPIO6/7 ICR register overflow -> memory corruption -> crash.
 *   HardwareInterface_MicroDexed uses polled Gray-code decode instead.
 */

#include <Audio.h>
#include <Wire.h>
#include <usb_midi.h>
#include <USBHost_t36.h>
#include "SynthEngine.h"
#include "UIPageLayout.h"
#include "HardwareInterface_MicroDexed.h"
#include "UIManager_TFT.h"
#include "Presets.h"
#include "AudioScopeTap.h"
#include "BPMClockManager.h"

// ---------------------------------------------------------------------------
// PCM5102A mute control
//   XSMT = HIGH -> DAC active
//   XSMT = LOW  -> DAC hardware muted (power-up default)
//   Wire this pin to the XSMT pad on your PCM5102A board.
// ---------------------------------------------------------------------------
static constexpr uint8_t DAC_MUTE_PIN = 34;  // Change to match your wiring

// ---------------------------------------------------------------------------
// Audio endpoints
// ---------------------------------------------------------------------------
AudioOutputI2S i2sOut;    // do not use I2S2 output -> PCM5102A (BCK=26, LRCK=27, DATA=7)
AudioInputUSB   usbIn;     // USB audio in  (for DAW loopback / monitoring)
AudioOutputUSB  usbOut;    // USB audio out
AudioScopeTap   scopeTap;  // Waveform capture for oscilloscope view

// ---------------------------------------------------------------------------
// Signal split: synth FX output fans to I2S path and USB path independently.
//   mixerI2S{L/R}: slot 0 = synth, slot 1 = USB audio in (pass-through)
// ---------------------------------------------------------------------------
AudioMixer4    mixerI2SL;
AudioMixer4    mixerI2SR;
AudioAmplifier ampUSBL;    // Separate gain trim for USB output
AudioAmplifier ampUSBR;

// Audio patch cords — heap-allocated, live for program lifetime
AudioConnection* patchMixerI2SL    = nullptr;  // Synth FX L -> I2S mixer L
AudioConnection* patchMixerI2SR    = nullptr;  // Synth FX R -> I2S mixer R
AudioConnection* patchUSBInL       = nullptr;  // USB in L   -> I2S mixer L slot 1
AudioConnection* patchUSBInR       = nullptr;  // USB in R   -> I2S mixer R slot 1
AudioConnection* patchOutL         = nullptr;  // I2S mixer L -> I2S out L
AudioConnection* patchOutR         = nullptr;  // I2S mixer R -> I2S out R
AudioConnection* patchAmpUSBL      = nullptr;  // Synth FX L -> USB amp L
AudioConnection* patchAmpUSBR      = nullptr;  // Synth FX R -> USB amp R
AudioConnection* patchOutUSBL      = nullptr;  // USB amp L  -> USB out L
AudioConnection* patchOutUSBR      = nullptr;  // USB amp R  -> USB out R
AudioConnection* patchOutScope     = nullptr;  // Synth FX L -> scope tap

// ---------------------------------------------------------------------------
// Core objects
// ---------------------------------------------------------------------------
SynthEngine                 synth;
HardwareInterface_MicroDexed hw;
UIManager_TFT               ui;
BPMClockManager             bpmClock;

// ---------------------------------------------------------------------------
// USB Host MIDI (for class-compliant USB MIDI controllers)
// ---------------------------------------------------------------------------
USBHost     myusb;
USBHub      hub1(myusb);
MIDIDevice  midiHost(myusb);

// ---------------------------------------------------------------------------
// CC change notifier — called by SynthEngine after every handled CC.
// Keeps the display in sync when MIDI arrives from external sources.
// ---------------------------------------------------------------------------
static void onParam(uint8_t cc, uint8_t val) {
    // Uncomment to log all CC changes to Serial:
    // Serial.printf("[NOTIFY] CC %u = %u\n", cc, val);
    (void)cc; (void)val;
}

// ---------------------------------------------------------------------------
// MIDI note handlers
// ---------------------------------------------------------------------------
void handleNoteOn(byte channel, byte note, byte velocity) {
    Serial.printf("NoteOn  ch=%d note=%d vel=%d\n", channel, note, velocity);
    synth.noteOn(note, velocity / 127.0f);
}

void handleNoteOff(byte channel, byte note, byte /*velocity*/) {
    Serial.printf("NoteOff ch=%d note=%d\n", channel, note);
    synth.noteOff(note);
}

void handleControlChange(byte channel, byte control, byte value) {
    Serial.printf("CC ch=%d ctrl=%d val=%d\n", channel, control, value);
    synth.handleControlChange(channel, control, value);
}

// ---------------------------------------------------------------------------
// MIDI clock / transport handlers
// ---------------------------------------------------------------------------
void handleMIDIClock()    { bpmClock.handleMIDIClock(); }
void handleMIDIStart()    { bpmClock.handleMIDIStart();    Serial.println("MIDI Start"); }
void handleMIDIStop()     { bpmClock.handleMIDIStop();     Serial.println("MIDI Stop");  }
void handleMIDIContinue() { bpmClock.handleMIDIContinue(); Serial.println("MIDI Cont");  }

/** Route MIDI real-time bytes (0xF8-0xFF) to transport handlers. */
void handleMIDIRealTime(uint8_t byte) {
    switch (byte) {
        case 0xF8: handleMIDIClock();    break;
        case 0xFA: handleMIDIStart();    break;
        case 0xFC: handleMIDIStop();     break;
        case 0xFB: handleMIDIContinue(); break;
    }
}

// ---------------------------------------------------------------------------
// setup()
// ---------------------------------------------------------------------------
void setup() {
    Serial.begin(115200);

    // --- PCM5102A unmute ---
    // Drive XSMT HIGH before I2S starts.  The PCM5102A holds its own internal
    // mute until it sees valid LRCK, but XSMT must be HIGH or the output stage
    // stays disabled regardless of I2S signal.
    pinMode(DAC_MUTE_PIN, OUTPUT);
    digitalWrite(DAC_MUTE_PIN, HIGH);  // Un-mute DAC — CRITICAL for audio output
    Serial.println("DAC unmuted");

    // --- Audio memory (must be before any audio object initialises) ---
    // 200 blocks * 128 samples * 2 bytes = 51,200 bytes DMAMEM.
    // Reduce if RAM is tight; 128 is safe minimum for 8 voices + FX.
    AudioMemory(200);

    // --- USB Host ---
    myusb.begin();

    // --- MIDI handlers: USB host device ---
    midiHost.setHandleNoteOn(handleNoteOn);
    midiHost.setHandleNoteOff(handleNoteOff);
    midiHost.setHandleControlChange(handleControlChange);
    midiHost.setHandleRealTimeSystem(handleMIDIRealTime);

    // --- MIDI handlers: Teensy USB-MIDI device ---
    usbMIDI.setHandleNoteOn(handleNoteOn);
    usbMIDI.setHandleNoteOff(handleNoteOff);
    usbMIDI.setHandleControlChange(handleControlChange);
    usbMIDI.setHandleRealTimeSystem(handleMIDIRealTime);

    // --- BPM clock ---
    bpmClock.setInternalBPM(120.0f);
    bpmClock.setClockSource(CLOCK_INTERNAL);
    synth.setBPMClock(&bpmClock);
    Serial.println("BPM clock: 120 BPM internal");

    // --- Audio patch cords ---
    // Synth FX stereo output -> I2S mixer
    patchMixerI2SL = new AudioConnection(synth.getFXOutL(), 0, mixerI2SL, 0);
    patchMixerI2SR = new AudioConnection(synth.getFXOutR(), 0, mixerI2SR, 0);

    // USB audio in -> I2S mixer (for external pass-through / monitoring)
    patchUSBInL    = new AudioConnection(usbIn, 0, mixerI2SL, 1);
    patchUSBInR    = new AudioConnection(usbIn, 1, mixerI2SR, 1);

    // I2S mixer -> I2S output (PCM5102A)
    patchOutL      = new AudioConnection(mixerI2SL, 0, i2sOut, 0);
    patchOutR      = new AudioConnection(mixerI2SR, 0, i2sOut, 1);

    // Synth FX stereo output -> USB output (DAW monitoring)
    patchAmpUSBL   = new AudioConnection(synth.getFXOutL(), 0, ampUSBL, 0);
    patchAmpUSBR   = new AudioConnection(synth.getFXOutR(), 0, ampUSBR, 0);
    patchOutUSBL   = new AudioConnection(ampUSBL, 0, usbOut, 0);
    patchOutUSBR   = new AudioConnection(ampUSBR, 0, usbOut, 1);

    // Scope tap (left channel only — sufficient for waveform display)
    patchOutScope  = new AudioConnection(synth.getFXOutL(), 0, scopeTap, 0);

    // --- Mixer/amp gains ---
    mixerI2SL.gain(0, 1.0f);   // Synth -> I2S L (full level)
    mixerI2SR.gain(0, 1.0f);   // Synth -> I2S R
    mixerI2SL.gain(1, 0.4f);   // USB in -> I2S L (reduced — avoid clipping with synth)
    mixerI2SR.gain(1, 0.4f);   // USB in -> I2S R
    ampUSBL.gain(0.7f);        // USB output trim (slightly below full to leave headroom)
    ampUSBR.gain(0.7f);

    // --- Hardware + UI init ---
    hw.begin();   // Sets up polled encoders and button pins
    ui.begin(synth);   // Display init, splash screen

    synth.setNotifier(onParam);

    // // Populate initial UI labels from current page
    // const int page = ui.getCurrentPage();
    // for (int i = 0; i < 4; ++i) {
    //     ui.setParameterLabel(i, UIPage::ccNames[page][i]);
    // }
    ui.syncFromEngine(synth);

    Serial.println("JTeensy 4000 ready");
}

// ---------------------------------------------------------------------------
// loop()
// ---------------------------------------------------------------------------
void loop() {
    // --- USB Host stack — must be called every loop() ---
    myusb.Task();

    // --- MIDI read: USB host device ---
    // midiHost.read() processes one message per call; loop until drained.
    while (midiHost.read()) {}

    // --- MIDI read: Teensy USB-MIDI device ---
    while (usbMIDI.read()) {}

    // --- Synth engine update (BPM sync, voice updates) ---
    synth.update();

    // --- Hardware: poll encoders + buttons ---
    // Must be called every loop() — polled Gray-code needs > 500 Hz.
    hw.update();

    // --- UI input: translate encoder/touch events to CC changes ---
    ui.pollInputs(hw, synth);

    // --- Display refresh (rate-limited internally to ~30 FPS) ---
    ui.updateDisplay(synth);
}
