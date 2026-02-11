/**
 * HardwareInterface_MicroDexed.h
 * 
 * Hardware abstraction for MicroDexed-style hardware:
 * - 2x rotary encoders with buttons (left/right)
 * - ILI9341 320x240 TFT display (SPI)
 * - PCM5102A I2S DAC
 * - No potentiometers (encoder-based parameter control)
 * 
 * Performance optimizations:
 * - Interrupt-driven encoders for zero-latency input
 * - Minimal polling overhead
 * - Button debouncing in hardware ISR
 */

#pragma once
#include <Arduino.h>
#include <EncoderTool.h>  // Interrupt-based encoder library
using namespace EncoderTool;

class HardwareInterface_MicroDexed {
public:
    // --- Encoder identifiers ---
    enum EncoderID {
        ENC_LEFT  = 0,  // Navigation encoder (back, cancel, parameter select)
        ENC_RIGHT = 1,  // Value encoder (confirm, adjust, select)
        ENC_COUNT = 2
    };

    // --- Button press types ---
    enum ButtonPress {
        PRESS_NONE  = 0,
        PRESS_SHORT = 1,  // Momentary press
        PRESS_LONG  = 2   // Hold for ~2 seconds
    };

    HardwareInterface_MicroDexed();
    
    /**
     * Initialize hardware:
     * - Configure encoder pins with interrupts
     * - Setup button pins with pull-ups
     * - No analog setup needed (no pots)
     */
    void begin();
    
    /**
     * Update button states and timing.
     * Call this in main loop at ~100Hz or faster for responsive buttons.
     * Encoders are interrupt-driven and don't need polling.
     */
    void update();

    // --- Encoder interface ---
    /**
     * Get encoder rotation delta since last call.
     * Returns: Positive for clockwise, negative for counter-clockwise.
     * Uses 4X counting (4 counts per detent).
     */
    int  getEncoderDelta(EncoderID encoder);
    
    /**
     * Check if encoder button was pressed (edge detection).
     * Returns: PRESS_SHORT, PRESS_LONG, or PRESS_NONE
     * Automatically clears the press state after reading.
     */
    ButtonPress getButtonPress(EncoderID encoder);

    /**
     * Check if encoder button is currently held down (level detection).
     * Useful for real-time hold detection without consuming the press.
     */
    bool isButtonHeld(EncoderID encoder);

    /**
     * Reset encoder count to zero.
     * Useful for mode changes or parameter boundaries.
     */
    void resetEncoder(EncoderID encoder);

private:
    // --- Pin definitions for MicroDexed Touch hardware ---
    // NOTE: These pins are based on common MicroDexed configurations.
    // You may need to adjust based on your specific build/PCB variant.
    
    // Left encoder (ENC_L): Navigation, back, cancel
    static constexpr uint8_t ENC_L_A_PIN  = 2;   // Phase A (interrupt-capable)
    static constexpr uint8_t ENC_L_B_PIN  = 3;   // Phase B (interrupt-capable)
    static constexpr uint8_t ENC_L_SW_PIN = 8;   // Button/switch

    // Right encoder (ENC_R): Value adjust, confirm, select
    static constexpr uint8_t ENC_R_A_PIN  = 4;   // Phase A (interrupt-capable)
    static constexpr uint8_t ENC_R_B_PIN  = 5;   // Phase B (interrupt-capable)
    static constexpr uint8_t ENC_R_SW_PIN = 6;   // Button/switch

    // --- Encoder objects ---
    // Using EncoderTool library for interrupt-driven, zero-copy reading
    Encoder _encoders[ENC_COUNT];
    int32_t _lastEncoderValues[ENC_COUNT] = {0, 0};

    // --- Button state tracking ---
    struct ButtonState {
        bool     current;       // Current physical button state (LOW = pressed)
        bool     lastState;     // Previous state for edge detection
        uint32_t pressTime;     // Millis when button was first pressed
        uint32_t releaseTime;   // Millis when button was released
        ButtonPress pendingPress; // Press type detected but not yet consumed
    };
    ButtonState _buttons[ENC_COUNT];

    // --- Button timing constants ---
    static constexpr uint32_t DEBOUNCE_MS = 50;    // Ignore bounces < 50ms
    static constexpr uint32_t LONG_PRESS_MS = 1500; // Hold for 1.5s = long press

    /**
     * Update a single button's state.
     * Handles debouncing and long press detection.
     */
    void updateButton(EncoderID encoder, uint8_t pin);
};
