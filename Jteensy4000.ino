#include <Audio.h>
#include <Wire.h>
#include <usb_midi.h>
#include <USBHost_t36.h>
#include "SynthEngine.h"
#include "HardwareInterface.h"
#include "UIManager.h"
#include "CCMap.h"
#include "Presets.h"

AudioOutputI2S i2sOut;
AudioOutputUSB usbOut;
AudioControlSGTL5000 audioShield;
AudioConnection* patchOutL;
AudioConnection* patchOutR;
AudioConnection* patchOutUSBL;
AudioConnection* patchOutUSBR;
AudioConnection* patchOutOsc;

SynthEngine synth;
HardwareInterface hw;
UIManager ui;

// --- USB Host MIDI ---
USBHost myusb;
USBHub hub1(myusb);       // If you ever use a USB hub
MIDIDevice midiHost(myusb);

unsigned long lastDisplayUpdate = 0;
const unsigned long displayRefreshMs = 100;

void handleNoteOn(byte channel, byte note, byte velocity) {
  float normvelocity = velocity / 127.0f;
  Serial.printf("handleNoteOn : %d vel %d\n", note, normvelocity);
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

// Example: type a number 0..8 in Serial Monitor and press Enter to load that template
void handleSerialPresets(SynthEngine& synth, UIManager& ui) {
  if (!Serial.available()) return;

  // Read an integer index (tolerates whitespace/newlines)
  int idx = Serial.parseInt();  // returns 0 if none; you can guard with isDigit peek if preferred
  if (idx >= 0 && idx <= 8) {
    Serial.printf("Loading template %d: %s\n", idx, Presets::templateName(idx));
    Presets::loadTemplateByIndex(synth, (uint8_t)idx);
    ui.syncFromEngine(synth);  // refresh screen to show current values
  } else {
    // Optional help
    Serial.println("Enter a preset index 0..8");
  }
}

void setup() {
  AudioMemory(200);

  // Start USB host
  myusb.begin();

  audioShield.enable();
  audioShield.volume(0.3);
  audioShield.lineOutLevel(0);
  audioShield.lineInLevel(0);
  // Attach MIDI host handlers
  midiHost.setHandleNoteOn(handleNoteOn);
  midiHost.setHandleNoteOff(handleNoteOff);
  midiHost.setHandleControlChange(handleControlChange);

  usbMIDI.setHandleNoteOn(handleNoteOn);
  usbMIDI.setHandleNoteOff(handleNoteOff);
  usbMIDI.setHandleControlChange(handleControlChange);

  Serial.println("JTeensy 4000 Synth Ready");

  synth.setOsc1Waveform(1);
  synth.setOsc2Waveform(2);
  patchOutL = new AudioConnection(synth.getFXOutL(), 0, i2sOut, 0);
  patchOutR = new AudioConnection(synth.getFXOutR(), 0, i2sOut, 1);
  patchOutUSBL = new AudioConnection(synth.getFXOutL(), 0, usbOut, 0);
  patchOutUSBR = new AudioConnection(synth.getFXOutR(), 0, usbOut, 1);
  //patchOutOsc = new AudioConnection(synth.getFXOutL(), 0, ui.getScopeInputStream(), 0);
  // After (refs -> correct)
  patchOutOsc = new AudioConnection(synth.getFXOutL(), 0, ui.scopeIn(), 0);

  hw.begin();
  ui.begin();

  int page = ui.getCurrentPage();
  for (int i = 0; i < 4; ++i) {
    ui.setParameterLabel(i, ccNames[page][i]);
  }
  ui.syncFromEngine(synth);
}

void loop() {
  usbMIDI.read();
  synth.update();
  hw.update();

  static unsigned long lastPerfPrint = 0;
  if (millis() - lastPerfPrint > 1000) {
    lastPerfPrint = millis();
        Serial.printf("Audio CPU: now=%u%%  max=%u%%  Mem: now=%u  max=%u\n", AudioProcessorUsage(), AudioProcessorUsageMax(), AudioMemoryUsage(), AudioMemoryUsageMax());
    AudioProcessorUsageMaxReset();
    AudioMemoryUsageMaxReset();
  }


  myusb.Task();
  midiHost.read();

  // Process USB device MIDI (your existing code)
  while (usbMIDI.read()) {
    byte type = usbMIDI.getType();
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
  // ✅ Poll encoder and pots
  ui.pollInputs(hw, synth);

  // ✅ Refresh OLED
  if (millis() - lastDisplayUpdate > displayRefreshMs) {
    ui.updateDisplay(synth);
    lastDisplayUpdate = millis();
  }

  handleSerialPresets(synth, ui);

}

