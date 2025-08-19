#include <Audio.h>
#include <Wire.h>
#include <usb_midi.h>
#include "SynthEngine.h"
#include "HardwareInterface.h"
#include "UIManager.h"
#include "CCMap.h"

AudioOutputI2S i2sOut;
AudioOutputUSB usbOut;
AudioControlSGTL5000 audioShield;
AudioConnection* patchOutL;
AudioConnection* patchOutR;
AudioConnection* patchOutUSBL;
AudioConnection* patchOutUSBR;

SynthEngine synth;
HardwareInterface hw;
UIManager ui;


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

void setup() {
  AudioMemory(200);

  audioShield.enable();
  audioShield.volume(0.3);
  audioShield.lineOutLevel(0);
  audioShield.lineInLevel(0);

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
    Serial.printf("[System] CPU: %.2f%%  Mem: %.0f\n", AudioProcessorUsage(), AudioMemoryUsage());
    AudioProcessorUsageMaxReset();
  }

  // ✅ Poll encoder and pots
  ui.pollInputs(hw, synth);

  // ✅ Refresh OLED
  if (millis() - lastDisplayUpdate > displayRefreshMs) {
    ui.updateDisplay(synth);
    lastDisplayUpdate = millis();
  }
}

