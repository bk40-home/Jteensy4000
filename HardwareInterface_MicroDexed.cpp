/**
 * HardwareInterface_MicroDexed.cpp
 *
 * Interrupt-driven encoder + debounced button implementation.
 * See header for architecture notes.
 */

#include "HardwareInterface_MicroDexed.h"

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

HardwareInterface_MicroDexed::HardwareInterface_MicroDexed() {
    for (int i = 0; i < ENC_COUNT; ++i) {
        _buttons[i].current = false;
        _buttons[i].lastState = false;
        _buttons[i].pressTime = 0;
        _buttons[i].releaseTime = 0;
        _buttons[i].pendingPress = PRESS_NONE;
    }
}

// ---------------------------------------------------------------------------
// begin()
// ---------------------------------------------------------------------------

void HardwareInterface_MicroDexed::begin() {

    // --- Left encoder (navigation) ---
    // CountMode::quarter = 1 logical count per physical detent (most natural for menus)
    _encoders[ENC_LEFT].begin(
        ENC_L_A_PIN,
        ENC_L_B_PIN,
        EncoderTool::CountMode::quarter,  // 1 increment per physical click
        INPUT_PULLUP                       // Enable internal pull-ups
    );
    pinMode(ENC_L_SW_PIN, INPUT_PULLUP);

    // --- Right encoder (value adjust) ---
    _encoders[ENC_RIGHT].begin(
        ENC_R_A_PIN, ENC_R_B_PIN,
        EncoderTool::CountMode::quarter,
        INPUT_PULLUP
    );
    pinMode(ENC_R_SW_PIN, INPUT_PULLUP);

    HW_LOG("Encoders initialised – L:[A=%d B=%d SW=%d] R:[A=%d B=%d SW=%d]",
           ENC_L_A_PIN, ENC_L_B_PIN, ENC_L_SW_PIN,
           ENC_R_A_PIN, ENC_R_B_PIN, ENC_R_SW_PIN);

    // -----------------------------------------------------------------------
    // ENCODER DEBUG: Verify interrupts fired up correctly.
    // Turn each encoder one click – you should see delta prints in Serial.
    // If delta is always 0, check:
    //   1. Pins wired correctly (A/B swapped causes wrong direction, not zero)
    //   2. EncoderTool version supports Teensy 4.x interrupts on these pins
    //   3. CountMode – try CountMode::full if quarter gives 0
    // -----------------------------------------------------------------------
}

// ---------------------------------------------------------------------------
// update() – button state machines only; encoders are interrupt-driven
// ---------------------------------------------------------------------------

void HardwareInterface_MicroDexed::update() {
    updateButton(ENC_LEFT,  ENC_L_SW_PIN);
    updateButton(ENC_RIGHT, ENC_R_SW_PIN);
}

// ---------------------------------------------------------------------------
// getEncoderDelta()
// ---------------------------------------------------------------------------

int HardwareInterface_MicroDexed::getEncoderDelta(EncoderID enc) {
    if (enc >= ENC_COUNT) return 0;

    // Read interrupt-maintained counter
    int32_t current = _encoders[enc].getValue();
    int32_t delta   = current - _lastEncoderValues[enc];
    _lastEncoderValues[enc] = current;

#if HW_DEBUG
    // Only print when something actually moved – avoids serial flood
    if (delta != 0) {
        HW_LOG("Encoder %s delta=%d  (raw=%d)",
               (enc == ENC_LEFT) ? "LEFT" : "RIGHT",
               (int)delta, (int)current);
    }
#endif

    return (int)delta;
}

// ---------------------------------------------------------------------------
// getButtonPress() – returns and clears pending press
// ---------------------------------------------------------------------------

HardwareInterface_MicroDexed::ButtonPress
HardwareInterface_MicroDexed::getButtonPress(EncoderID enc) {
    if (enc >= ENC_COUNT) return PRESS_NONE;

    ButtonPress press = _buttons[enc].pendingPress;
    _buttons[enc].pendingPress = PRESS_NONE;

#if HW_DEBUG
    if (press != PRESS_NONE) {
        HW_LOG("Button %s press: %s",
               (enc == ENC_LEFT) ? "LEFT" : "RIGHT",
               (press == PRESS_SHORT) ? "SHORT" : "LONG");
    }
#endif

    return press;
}

// ---------------------------------------------------------------------------
// isButtonHeld()
// ---------------------------------------------------------------------------

bool HardwareInterface_MicroDexed::isButtonHeld(EncoderID enc) {
    if (enc >= ENC_COUNT) return false;
    return _buttons[enc].current;
}

// ---------------------------------------------------------------------------
// resetEncoder()
// ---------------------------------------------------------------------------

void HardwareInterface_MicroDexed::resetEncoder(EncoderID enc) {
    if (enc >= ENC_COUNT) return;
    _encoders[enc].setValue(0);
    _lastEncoderValues[enc] = 0;
    HW_LOG("Encoder %s reset", (enc == ENC_LEFT) ? "LEFT" : "RIGHT");
}

// ---------------------------------------------------------------------------
// updateButton() – internal: debounce + long-press classification
// ---------------------------------------------------------------------------

void HardwareInterface_MicroDexed::updateButton(EncoderID enc, uint8_t pin) {
    ButtonState& btn = _buttons[enc];
    const uint32_t now = millis();

    // INPUT_PULLUP: pin reads LOW when button is pressed
    const bool pressed = (digitalRead(pin) == LOW);
    btn.current = pressed;

    if (pressed && !btn.lastState) {
        // --- Falling edge: button just pressed ---
        if ((now - btn.releaseTime) > DEBOUNCE_MS) {
            btn.pressTime = now;   // Latch start time for duration measurement
        }
    }
    else if (!pressed && btn.lastState) {
        // --- Rising edge: button just released ---
        const uint32_t duration = now - btn.pressTime;

        if (duration > DEBOUNCE_MS) {          // Filter mechanical bounce
            btn.releaseTime = now;
            // Only classify on release if a long-press wasn't already fired mid-hold
            if (btn.pendingPress == PRESS_NONE) {
                btn.pendingPress = (duration >= LONG_PRESS_MS) ? PRESS_LONG : PRESS_SHORT;
            }
        }
    }
    else if (pressed && btn.lastState) {
        // --- Button held: fire long-press early (before release) ---
        // Allows UI to react immediately at the threshold rather than waiting for lift
        if ((now - btn.pressTime) >= LONG_PRESS_MS &&
            btn.pendingPress == PRESS_NONE) {
            btn.pendingPress = PRESS_LONG;
        }
    }

    btn.lastState = pressed;
}