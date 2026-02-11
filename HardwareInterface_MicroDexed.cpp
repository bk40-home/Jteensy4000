/**
 * HardwareInterface_MicroDexed.cpp
 * 
 * Efficient hardware interface implementation:
 * - Interrupt-driven encoders (no polling CPU overhead)
 * - Debounced button handling with long-press detection
 * - Minimal update() overhead (only button state machines)
 */

#include "HardwareInterface_MicroDexed.h"

HardwareInterface_MicroDexed::HardwareInterface_MicroDexed() {
    // Initialize button states
    for (int i = 0; i < ENC_COUNT; ++i) {
        _buttons[i].current = false;
        _buttons[i].lastState = false;
        _buttons[i].pressTime = 0;
        _buttons[i].releaseTime = 0;
        _buttons[i].pendingPress = PRESS_NONE;
    }
}

void HardwareInterface_MicroDexed::begin() {
    // --- Initialize left encoder (ENC_L) ---
    // CountMode::quarter = 1 count per detent (most intuitive for UI navigation)
    _encoders[ENC_LEFT].begin(
        ENC_L_A_PIN,
        ENC_L_B_PIN,
        EncoderTool::CountMode::quarter,  // 1 increment per physical click
        INPUT_PULLUP                       // Enable internal pull-ups
    );
    
    // Configure left encoder button
    pinMode(ENC_L_SW_PIN, INPUT_PULLUP);

    // --- Initialize right encoder (ENC_R) ---
    _encoders[ENC_RIGHT].begin(
        ENC_R_A_PIN,
        ENC_R_B_PIN,
        EncoderTool::CountMode::quarter,
        INPUT_PULLUP
    );
    
    // Configure right encoder button
    pinMode(ENC_R_SW_PIN, INPUT_PULLUP);

    // Note: Encoders are now fully interrupt-driven.
    // No polling required in update() for rotation detection.
}

void HardwareInterface_MicroDexed::update() {
    // Only update button states (encoders are interrupt-driven)
    // This is very lightweight - just button debouncing and timing
    updateButton(ENC_LEFT,  ENC_L_SW_PIN);
    updateButton(ENC_RIGHT, ENC_R_SW_PIN);
}

int HardwareInterface_MicroDexed::getEncoderDelta(EncoderID encoder) {
    if (encoder >= ENC_COUNT) return 0;

    // Read current encoder value (interrupt-maintained count)
    int32_t current = _encoders[encoder].getValue();
    
    // Calculate delta since last read
    int delta = current - _lastEncoderValues[encoder];
    
    // Update last known value
    _lastEncoderValues[encoder] = current;
    
    return delta;
}

HardwareInterface_MicroDexed::ButtonPress HardwareInterface_MicroDexed::getButtonPress(EncoderID encoder) {
    if (encoder >= ENC_COUNT) return PRESS_NONE;

    // Return and consume pending press
    ButtonPress press = _buttons[encoder].pendingPress;
    _buttons[encoder].pendingPress = PRESS_NONE;
    return press;
}

bool HardwareInterface_MicroDexed::isButtonHeld(EncoderID encoder) {
    if (encoder >= ENC_COUNT) return false;
    return _buttons[encoder].current;
}

void HardwareInterface_MicroDexed::resetEncoder(EncoderID encoder) {
    if (encoder >= ENC_COUNT) return;
    
    // Reset interrupt-driven encoder to zero
    _encoders[encoder].setValue(0);
    _lastEncoderValues[encoder] = 0;
}

void HardwareInterface_MicroDexed::updateButton(EncoderID encoder, uint8_t pin) {
    ButtonState& btn = _buttons[encoder];
    uint32_t now = millis();

    // Read current button state (LOW = pressed due to INPUT_PULLUP)
    bool pressed = (digitalRead(pin) == LOW);
    btn.current = pressed;

    // --- Detect button press (falling edge) ---
    if (pressed && !btn.lastState) {
        // Button was just pressed
        uint32_t timeSinceRelease = now - btn.releaseTime;
        
        // Debounce: ignore if released very recently
        if (timeSinceRelease > DEBOUNCE_MS) {
            btn.pressTime = now;
        }
    }
    
    // --- Detect button release (rising edge) ---
    else if (!pressed && btn.lastState) {
        // Button was just released
        uint32_t pressDuration = now - btn.pressTime;
        
        // Debounce: ignore very short presses
        if (pressDuration > DEBOUNCE_MS) {
            btn.releaseTime = now;
            
            // Classify press type based on duration
            if (pressDuration >= LONG_PRESS_MS) {
                btn.pendingPress = PRESS_LONG;
            } else {
                btn.pendingPress = PRESS_SHORT;
            }
        }
    }
    
    // --- Detect long press while still held ---
    // This allows UI to react to long press before release
    else if (pressed && btn.lastState) {
        uint32_t pressDuration = now - btn.pressTime;
        
        // Transition to long press state once threshold is crossed
        // Only trigger once per hold (check if not already triggered)
        if (pressDuration >= LONG_PRESS_MS && btn.pendingPress == PRESS_NONE) {
            btn.pendingPress = PRESS_LONG;
        }
    }

    btn.lastState = pressed;
}
