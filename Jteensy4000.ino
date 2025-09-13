#include <Audio.h>
#include <Wire.h>
#include <usb_midi.h>
#include <USBHost_t36.h>
#include "SynthEngine.h"
#include "HardwareInterface.h"
#include "UIManager.h"
#include "CCMap.h"
#include "Presets.h"

// ---------------------- Audio endpoints ----------------------
AudioOutputI2S2 i2sOut;                 // I2S audio out (to SGTL5000 / PCM path)
AudioOutputUSB usbOut;                 // USB audio out
AudioControlSGTL5000 audioShield;      // Audio shield control

// Patch-cord pointers (kept alive for the program lifetime)
AudioConnection* patchAmpI2SL = nullptr;
AudioConnection* patchAmpI2SR = nullptr;
AudioConnection* patchOutL    = nullptr;
AudioConnection* patchOutR    = nullptr;

AudioConnection* patchAmpUSBL = nullptr;
AudioConnection* patchAmpUSBR = nullptr;
AudioConnection* patchOutUSBL = nullptr;
AudioConnection* patchOutUSBR = nullptr;

AudioConnection* patchOutOsc  = nullptr;   // scope feed

// ---------------------- Engine & UI --------------------------
SynthEngine synth;
HardwareInterface hw;
UIManager ui;

// Split to 2 independent paths with their own amps (objects, not functions)
AudioAmplifier  ampI2SL;
AudioAmplifier  ampI2SR;
AudioAmplifier  ampUSBL;
AudioAmplifier  ampUSBR;

// ---------------------- USB Host MIDI ------------------------
USBHost myusb;
USBHub hub1(myusb);       // if a hub is used
MIDIDevice midiHost(myusb);

unsigned long lastDisplayUpdate = 0;
const unsigned long displayRefreshMs = 100;

// ---------------------- MIDI handlers ------------------------
void handleNoteOn(byte channel, byte note, byte velocity) {
  float normvelocity = velocity / 127.0f;
  Serial.printf("handleNoteOn : %d vel %0.2f\n", note, normvelocity);
  synth.noteOn(note, velocity);
}

void handleNoteOff(byte channel, byte note, byte velocity) {
  Serial.printf("handleNoteOff: %d\n", note);
  synth.noteOff(note);
}

void handleControlChange(byte channel, byte control, byte value) {
  Serial.printf("MIDI CC: ch=%d ctrl=%d val=%d\n", channel, control, value);
  synth.handleControlChange(channel, control, value);
}

// Example: type a number 0..8 in Serial Monitor and press Enter to load template
void handleSerialPresets(SynthEngine& synth, UIManager& ui) {
  if (!Serial.available()) return;
  int idx = Serial.parseInt();
  if (idx >= 0 && idx <= 8) {
    Serial.printf("Loading template %d: %s\n", idx, Presets::templateName(idx));
    Presets::loadTemplateByIndex(synth, (uint8_t)idx);
    ui.syncFromEngine(synth);
  } else {
    Serial.println("Enter a preset index 0..8");
  }
}

void setup() {
  Serial.begin(115200);                 // debug prints
  AudioMemory(200);                     // audio buffers before starting graph

  // Start USB host
  myusb.begin();

  // Audio shield config
  audioShield.enable();
  audioShield.volume(0.3f);
  audioShield.lineOutLevel(0);
  audioShield.lineInLevel(0);

  // Attach MIDI host handlers
  midiHost.setHandleNoteOn(handleNoteOn);
  midiHost.setHandleNoteOff(handleNoteOff);
  midiHost.setHandleControlChange(handleControlChange);

  // USB device MIDI handlers
  usbMIDI.setHandleNoteOn(handleNoteOn);
  usbMIDI.setHandleNoteOff(handleNoteOff);
  usbMIDI.setHandleControlChange(handleControlChange);

  Serial.println("JTeensy 4000 Synth Ready");

  // ---------------------- AUDIO PATCHING ----------------------
  // IMPORTANT:
  // - For getters that return AudioStream& (e.g., getFXOutL(), ui.scopeIn()), call them with ().
  // - For actual objects (ampI2SL/ampI2SR/i2sOut/usbOut), pass the object (NO parentheses).

  // FX L/R -> I2S amp L/R
  patchAmpI2SL = new AudioConnection(synth.getFXOutL(), 0, ampI2SL, 0);
  patchAmpI2SR = new AudioConnection(synth.getFXOutR(), 0, ampI2SR, 0);

  // FX L/R -> USB amp L/R
  patchAmpUSBL = new AudioConnection(synth.getFXOutL(), 0, ampUSBL, 0);
  patchAmpUSBR = new AudioConnection(synth.getFXOutR(), 0, ampUSBR, 0);

  // I2S amp L/R -> I2S out L/R
  patchOutL    = new AudioConnection(ampI2SL, 0, i2sOut, 0);
  patchOutR    = new AudioConnection(ampI2SR, 0, i2sOut, 1);

  // USB amp L/R -> USB out L/R
  patchOutUSBL = new AudioConnection(ampUSBL, 0, usbOut, 0);
  patchOutUSBR = new AudioConnection(ampUSBR, 0, usbOut, 1);

  // Scope tap (FX L -> UI scope input)
  // ui.scopeIn() must return an AudioStream& (e.g., an AudioAnalyze or custom scope sink)
  patchOutOsc  = new AudioConnection(synth.getFXOutL(), 0, ui.scopeIn(), 0);

  // ---------------------- INIT UI/HW --------------------------
  hw.begin();
  ui.begin();

  // Split-path gains (will later map to a master volume)
  ampI2SL.gain(0.3f);
  ampI2SR.gain(0.3f);
  ampUSBL.gain(0.3f);
  ampUSBR.gain(0.3f);

  int page = ui.getCurrentPage();
  for (int i = 0; i < 4; ++i) {
    ui.setParameterLabel(i, ccNames[page][i]);
  }
  ui.syncFromEngine(synth);
}

void loop() {
  // USB device MIDI
  usbMIDI.read();

  // Engine + hardware
  synth.update();
  hw.update();

  // USB host stack and MIDI host
  myusb.Task();
  midiHost.read();

  // Process USB device MIDI manually (if needed by your flow)
  while (usbMIDI.read()) {
    byte type  = usbMIDI.getType();
    byte data1 = usbMIDI.getData1();
    byte data2 = usbMIDI.getData2();

    if (type == usbMIDI.NoteOn && data2 > 0) {
      handleNoteOn(usbMIDI.getChannel(), data1, data2);
    } else if (type == usbMIDI.NoteOff || (type == usbMIDI.NoteOn && data2 == 0)) {
      handleNoteOff(usbMIDI.getChannel(), data1, data2);
    } else if (type == usbMIDI.ControlChange) {
      handleControlChange(usbMIDI.getChannel(), data1, data2);
    }
  }

  // UI input polling (encoder + 4 pots)
  ui.pollInputs(hw, synth);

  // OLED refresh
  if (millis() - lastDisplayUpdate > displayRefreshMs) {
    ui.updateDisplay(synth);
    lastDisplayUpdate = millis();
  }

  // Serial-driven preset loader
  handleSerialPresets(synth, ui);
}
