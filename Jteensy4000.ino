/**
 * Jteensy4000.ino — JT-4000 polyphonic synthesizer main sketch
 *
 * Audio path:
 *   SynthEngine (8 voices) -> FXChainBlock -> AudioMixer4 (I2S split)
 *                                           -> AudioAmplifier  (USB split)
 *   I2S output: AudioOutputI2S -> PCM5102A DAC
 *   USB output: AudioOutputUSB  (for DAW monitoring)
 *
 * PCM5102A DAC notes:
 *   XSMT (mute) pin MUST be driven HIGH after I2S starts or DAC stays silent.
 *   FMT  pin = LOW  -> I2S format (default)
 *   SCK  pin = LOW  -> no master clock needed
 *   Pin used: DAC_MUTE_PIN (34) — wire to PCM5102A XSMT.
 *
 * Encoder notes:
 *   Pins 28-32 must NOT use attachInterrupt() on Teensy 4.1.
 *   GPIO6/7 ICR register overflow -> memory corruption -> crash.
 *   HardwareInterface_MicroDexed uses polled Gray-code decode instead.
 *
 * UI setup sequence (order matters):
 *   1. ui.beginDisplay()  — SPI init + splash, BEFORE AudioMemory
 *   2. AudioMemory(256)   — audio pool, after SPI is stable
 *   3. synth, MIDI, hw init
 *   4. ui.begin(synth)    — wire screens, needs synth reference
 *   5. unmute DAC         — last, after I2S is running
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
// ---------------------------------------------------------------------------
static constexpr uint8_t DAC_MUTE_PIN = 34;

// ---------------------------------------------------------------------------
// Audio endpoints
// ---------------------------------------------------------------------------
AudioOutputI2S  i2sOut;    // I2S1: BCK=21, LRCK=20, DATA=7 -> PCM5102A
AudioInputUSB   usbIn;     // USB audio in  (for DAW loopback)
AudioOutputUSB  usbOut;    // USB audio out (for DAW monitoring)
AudioScopeTap   scopeTap;  // Waveform capture for oscilloscope view

// ---------------------------------------------------------------------------
// Signal split: synth FX -> I2S path and USB path independently.
//   mixerI2S{L/R}: slot 0 = synth, slot 1 = USB audio in (pass-through)
// ---------------------------------------------------------------------------
AudioMixer4    mixerI2SL;
AudioMixer4    mixerI2SR;
AudioAmplifier ampUSBL;    // Separate gain trim for USB output
AudioAmplifier ampUSBR;

// Audio patch cords — heap-allocated, live for program lifetime
AudioConnection* patchMixerI2SL = nullptr;  // Synth FX L -> I2S mixer L
AudioConnection* patchMixerI2SR = nullptr;  // Synth FX R -> I2S mixer R
AudioConnection* patchUSBInL    = nullptr;  // USB in L   -> I2S mixer L slot 1
AudioConnection* patchUSBInR    = nullptr;  // USB in R   -> I2S mixer R slot 1
AudioConnection* patchOutL      = nullptr;  // I2S mixer L -> I2S out L
AudioConnection* patchOutR      = nullptr;  // I2S mixer R -> I2S out R
AudioConnection* patchAmpUSBL   = nullptr;  // Synth FX L -> USB amp L
AudioConnection* patchAmpUSBR   = nullptr;  // Synth FX R -> USB amp R
AudioConnection* patchOutUSBL   = nullptr;  // USB amp L  -> USB out L
AudioConnection* patchOutUSBR   = nullptr;  // USB amp R  -> USB out R
AudioConnection* patchOutScope  = nullptr;  // Synth FX L -> scope tap

// ---------------------------------------------------------------------------
// Core objects
// ---------------------------------------------------------------------------
SynthEngine                  synth;
HardwareInterface_MicroDexed hw;
UIManager_TFT                ui;
BPMClockManager              bpmClock;

// ---------------------------------------------------------------------------
// USB Host MIDI
// ---------------------------------------------------------------------------
USBHost    myusb;
USBHub     hub1(myusb);
MIDIDevice midiHost(myusb);

// ---------------------------------------------------------------------------
// CC change notifier — called by SynthEngine after every handled CC.
// ---------------------------------------------------------------------------
static void onParam(uint8_t cc, uint8_t val) {
    (void)cc; (void)val;
    // Uncomment to log: Serial.printf("[NOTIFY] CC %u = %u\n", cc, val);
}

// ---------------------------------------------------------------------------
// MIDI handlers
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

void handleMIDIClock()    { bpmClock.handleMIDIClock(); }
void handleMIDIStart()    { bpmClock.handleMIDIStart();    Serial.println("MIDI Start"); }
void handleMIDIStop()     { bpmClock.handleMIDIStop();     Serial.println("MIDI Stop");  }
void handleMIDIContinue() { bpmClock.handleMIDIContinue(); Serial.println("MIDI Cont");  }

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
    // Let the power rail settle before touching any SPI peripherals
    delay(200);

    Serial.println("1");

    // STEP 1: Display hardware init (SPI) — must happen BEFORE AudioMemory.
    // Doing it after causes SPI1 to conflict with the audio DMA scheduler.
    ui.beginDisplay();
    Serial.println("2");

    // STEP 2: Audio memory — allocate after SPI is stable.
    // 256 blocks = 65,536 bytes DMAMEM. 200 was marginal with ~192 audio objects.
    AudioMemory(200);

    // STEP 3: USB Host + MIDI handlers
    myusb.begin();
    midiHost.setHandleNoteOn(handleNoteOn);
    midiHost.setHandleNoteOff(handleNoteOff);
    midiHost.setHandleControlChange(handleControlChange);
    midiHost.setHandleRealTimeSystem(handleMIDIRealTime);

    usbMIDI.setHandleNoteOn(handleNoteOn);
    usbMIDI.setHandleNoteOff(handleNoteOff);
    usbMIDI.setHandleControlChange(handleControlChange);
    usbMIDI.setHandleRealTimeSystem(handleMIDIRealTime);

    hw.begin();

    // STEP 4: Wire UI screens to synth (needs synth reference)
    ui.begin(synth);

    // STEP 5: Unmute DAC — AFTER I2S is running and outputting silence
    pinMode(DAC_MUTE_PIN, OUTPUT);
    digitalWrite(DAC_MUTE_PIN, HIGH);
    Serial.println("DAC unmuted");

    synth.setNotifier(onParam);
    ui.syncFromEngine(synth);

    // BPM clock
    bpmClock.setInternalBPM(120.0f);
    bpmClock.setClockSource(CLOCK_INTERNAL);
    synth.setBPMClock(&bpmClock);
    Serial.println("BPM clock: 120 BPM internal");

    // STEP 6: Audio patch cords (after AudioMemory)
    patchMixerI2SL = new AudioConnection(synth.getFXOutL(), 0, mixerI2SL, 0);
    patchMixerI2SR = new AudioConnection(synth.getFXOutR(), 0, mixerI2SR, 0);
    patchUSBInL    = new AudioConnection(usbIn, 0, mixerI2SL, 1);
    patchUSBInR    = new AudioConnection(usbIn, 1, mixerI2SR, 1);
    patchOutL      = new AudioConnection(mixerI2SL, 0, i2sOut, 0);
    patchOutR      = new AudioConnection(mixerI2SR, 0, i2sOut, 1);
    patchAmpUSBL   = new AudioConnection(synth.getFXOutL(), 0, ampUSBL, 0);
    patchAmpUSBR   = new AudioConnection(synth.getFXOutR(), 0, ampUSBR, 0);
    patchOutUSBL   = new AudioConnection(ampUSBL, 0, usbOut, 0);
    patchOutUSBR   = new AudioConnection(ampUSBR, 0, usbOut, 1);
    patchOutScope  = new AudioConnection(synth.getFXOutL(), 0, scopeTap, 0);

    // Mixer/amp gains
    mixerI2SL.gain(0, 1.0f);    // Synth -> I2S L (full level)
    mixerI2SR.gain(0, 1.0f);    // Synth -> I2S R
    mixerI2SL.gain(1, 0.4f);    // USB in -> I2S L (reduced to avoid clipping with synth)
    mixerI2SR.gain(1, 0.4f);    // USB in -> I2S R
    ampUSBL.gain(0.7f);         // USB output trim
    ampUSBR.gain(0.7f);

    Serial.println("JTeensy 4000 ready");
}

// ---------------------------------------------------------------------------
// loop()
// ---------------------------------------------------------------------------
void loop() {
    myusb.Task();

    while (midiHost.read()) {}
    while (usbMIDI.read()) {}

    synth.update();
    hw.update();
    ui.pollInputs(hw, synth);
    ui.updateDisplay(synth);
}
