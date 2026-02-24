/**
 * HardwareInterface_MicroDexed.h
 *
 * Hardware abstraction for MicroDexed-style hardware:
 *   - 2x rotary encoders with push buttons (left / right)
 *   - ILI9341 320×240 TFT display via SPI1 (MOSI=26, SCK=27, MISO=39)
 *   - PCM5102A I2S DAC
 *
 * Performance notes:
 *   - Encoders are fully interrupt-driven (EncoderTool) – zero polling overhead
 *   - update() only runs the lightweight button state machines
 *   - All timing constants are compile-time constexpr
 */

#pragma once
#include <Arduino.h>
#include <EncoderTool.h>   // Interrupt-based encoder library
using namespace EncoderTool;

// ---------------------------------------------------------------------------
// Debug switch – set to 1 to enable Serial diagnostics, 0 for release build
// ---------------------------------------------------------------------------
#define HW_DEBUG 1

#if HW_DEBUG
  #define HW_LOG(fmt, ...) Serial.printf("[HW] " fmt "\n", ##__VA_ARGS__)
#else
  #define HW_LOG(fmt, ...) /* nothing */
#endif

// ---------------------------------------------------------------------------

class HardwareInterface_MicroDexed {
public:

    // -----------------------------------------------------------------------
    // Types
    // -----------------------------------------------------------------------

    enum EncoderID : uint8_t {
        ENC_LEFT  = 0,   // Navigation – back / cancel / parameter select
        ENC_RIGHT = 1,   // Value     – confirm / adjust / select
        ENC_COUNT = 2
    };

    enum ButtonPress : uint8_t {
        PRESS_NONE  = 0,
        PRESS_SHORT = 1,  // Tap  (< LONG_PRESS_MS)
        PRESS_LONG  = 2   // Hold (≥ LONG_PRESS_MS)
    };

    // -----------------------------------------------------------------------
    // Lifecycle
    // -----------------------------------------------------------------------

    HardwareInterface_MicroDexed();

    /**
     * Initialise encoder interrupts and button pull-ups.
     * Call once from setup().
     */
    void begin();

    /**
     * Run button state machines.
     * Encoders are interrupt-driven so no rotation polling is needed here.
     * Call at ≥ 100 Hz for responsive button response.
     */
    void update();

    // -----------------------------------------------------------------------
    // Encoder interface
    // -----------------------------------------------------------------------

    /**
     * Return rotation delta since last call.
     * Positive = clockwise, negative = counter-clockwise.
     * Thread-safe: reads interrupt-maintained counter atomically.
     */
    int  getEncoderDelta(EncoderID enc);

    /**
     * Return and consume any pending button press.
     * Returns PRESS_NONE if nothing is waiting.
     */
    ButtonPress getButtonPress(EncoderID enc);

    /** True while button is physically held (level, not edge). */
    bool isButtonHeld(EncoderID enc);

    /** Reset encoder count to zero (e.g. on mode change). */
    void resetEncoder(EncoderID enc);

    // -----------------------------------------------------------------------
    // SPI pin definitions – exposed so UIManager can pass them to ILI9341_t3
    // -----------------------------------------------------------------------
    //
    // *** ROOT CAUSE OF BLANK DISPLAY ***
    // ILI9341_t3 defaults to the primary SPI bus (MOSI=11, SCK=13).
    // This hardware uses the SPI1 bus: MOSI=26, SCK=27, MISO=39.
    // These must be passed explicitly to the ILI9341_t3 constructor.
    //
    static constexpr uint8_t TFT_CS   = 41;  // Chip select
    static constexpr uint8_t TFT_DC   = 37;  // Data / command
    static constexpr uint8_t TFT_RST  = 24;  // Reset (or tie to 3.3 V)
    static constexpr uint8_t TFT_MOSI = 26;  // SPI1 MOSI  ← non-default, must be explicit
    static constexpr uint8_t TFT_SCK  = 27;  // SPI1 SCK   ← non-default, must be explicit
    static constexpr uint8_t TFT_MISO = 39;  // SPI1 MISO  ← non-default, must be explicit

private:

    // -----------------------------------------------------------------------
    // Encoder pin definitions
    // -----------------------------------------------------------------------

    // Left encoder – navigation
    // WARNING: Pins 28-32 on Teensy 4.1 have a known attachInterrupt() bug
    // (wrong ICR register for pins with index >= 31) that can crash the MCU.
    // EncoderTool uses interrupts internally. If the left encoder causes a crash,
    // replace Encoder with PollEncoder (see HardwareTest3) and use any pin safely.
    // Confirmed working wiring: A=32, B=31, SW=30
    static constexpr uint8_t ENC_L_A_PIN  = 32;
    static constexpr uint8_t ENC_L_B_PIN  = 31;
    static constexpr uint8_t ENC_L_SW_PIN = 30;

    // Right encoder – value adjust
    // Confirmed working wiring: A=28, B=29, SW=25
    static constexpr uint8_t ENC_R_A_PIN  = 28;
    static constexpr uint8_t ENC_R_B_PIN  = 29;
    static constexpr uint8_t ENC_R_SW_PIN = 25;

    // -----------------------------------------------------------------------
    // Encoder objects
    // -----------------------------------------------------------------------

    Encoder _encoders[ENC_COUNT];
    int32_t _lastEncoderValues[ENC_COUNT] = {0, 0};

    // -----------------------------------------------------------------------
    // Button state
    // -----------------------------------------------------------------------

    struct ButtonState {
        bool        current;       // Current physical level (LOW = pressed)
        bool        lastState;     // Previous level for edge detection
        uint32_t    pressTime;     // millis() when first pressed
        uint32_t    releaseTime;   // millis() when released
        ButtonPress pendingPress;  // Classified press waiting to be consumed
    };

    ButtonState _buttons[ENC_COUNT];

    // -----------------------------------------------------------------------
    // Timing constants
    // -----------------------------------------------------------------------

    static constexpr uint32_t DEBOUNCE_MS    =   50;   // Reject bounces shorter than this
    static constexpr uint32_t LONG_PRESS_MS  = 1500;   // Hold threshold for long press

    // -----------------------------------------------------------------------
    // Internal helpers
    // -----------------------------------------------------------------------

    void updateButton(EncoderID enc, uint8_t pin);
};