/**
 * TouchInput.cpp
 *
 * Implementation of touch input and gesture recognition.
 * Separated from TouchInput.h to prevent "multiple definition"
 * linker errors when included in more than one translation unit.
 *
 * Gesture thresholds (TAP / HOLD / SWIPE) are constexpr in TouchInput.h.
 * Coordinate calibration values are in mapCoordinates() below.
 */

#include "TouchInput.h"

// ============================================================================
// Constructor
// ============================================================================

TouchInput::TouchInput()
    : _isTouched(false)
    , _detectedGesture(GESTURE_NONE)
    , _touchStartTime(0)
    , _touchEndTime(0)
{}


// ============================================================================
// begin() — hardware initialisation
// ============================================================================

bool TouchInput::begin() {


    Serial.print("TouchInput: Initializing FT6206 (capacitive)... ");


    if (!_touchController.begin(40)) {
        Serial.println("FAILED - Not found on I2C");
        return false;
    }
    Serial.println("SUCCESS");
    return true;


}

// ============================================================================
// update() — poll hardware, maintain state, fire gesture detection
// ============================================================================

void TouchInput::update() {
    bool nowTouched = false;
    int16_t rawX = 0, rawY = 0;


    if (_touchController.touched()) {
        TS_Point p = _touchController.getPoint();
        rawX = p.x;
        rawY = p.y;
        nowTouched = true;
    }

    if (nowTouched) {
        _currentPoint = mapCoordinates(rawX, rawY);
            Serial.print("RAW: ");
    Serial.print(rawX);
    Serial.print(",");
    Serial.println(rawY);

        if (!_isTouched) {
            // Finger just landed
            _isTouched       = true;
            _gestureStart    = _currentPoint;
            _touchStartTime  = millis();
            _detectedGesture = GESTURE_NONE;
        }
        _lastPoint = _currentPoint;

    } else {
        if (_isTouched) {
            // Finger just lifted
            _isTouched   = false;
            _touchEndTime = millis();
            detectGesture();
        }
    }
}

// ============================================================================
// Public accessors
// ============================================================================

TouchInput::Point TouchInput::getTouchPoint() const {
    return _currentPoint;
}

TouchInput::Gesture TouchInput::getGesture() {
    Gesture g        = _detectedGesture;
    _detectedGesture = GESTURE_NONE;   // Consume — only fires once
    return g;
}

bool TouchInput::hitTest(int16_t x, int16_t y, int16_t w, int16_t h) const {
    if (!_isTouched) return false;
    return (_currentPoint.x >= x && _currentPoint.x < x + w &&
            _currentPoint.y >= y && _currentPoint.y < y + h);
}

// ============================================================================
// detectGesture() — classify a completed touch
// ============================================================================

void TouchInput::detectGesture() {
    const uint32_t duration = _touchEndTime - _touchStartTime;

    // Integer displacement — keep same type (int16_t) to avoid sign-compare
    const int16_t dx = _lastPoint.x - _gestureStart.x;
    const int16_t dy = _lastPoint.y - _gestureStart.y;

    // Distance as int16_t — sqrt result fits comfortably in 16 bits for a 320x240 screen
    const int16_t distance = (int16_t)sqrt((float)(dx * dx + dy * dy));

    if (distance < 10 && duration < TAP_MAX_DURATION) {
        // Short, stationary touch → TAP
        _detectedGesture = GESTURE_TAP;

    } else if (distance < 10 && duration >= HOLD_MIN_DURATION) {
        // Long stationary touch → HOLD
        _detectedGesture = GESTURE_HOLD;

    } else if (distance >= SWIPE_MIN_DISTANCE && duration < SWIPE_MAX_DURATION) {
        // Fast movement → SWIPE; dominant axis decides direction
        if (abs(dx) > abs(dy)) {
            _detectedGesture = (dx > 0) ? GESTURE_SWIPE_RIGHT : GESTURE_SWIPE_LEFT;
        } else {
            _detectedGesture = (dy > 0) ? GESTURE_SWIPE_DOWN  : GESTURE_SWIPE_UP;
        }

    } else {
        _detectedGesture = GESTURE_NONE;
    }
}

// ============================================================================
// mapCoordinates() — raw hardware → screen (landscape 320×240)
// ============================================================================

TouchInput::Point TouchInput::mapCoordinates(int16_t rawX, int16_t rawY) {

    // // FT6206 native resolution is typically 240×320 (portrait).
    // // In landscape mode the axes are swapped: raw X → screen Y, raw Y → screen X.
    // // Adjust the input ranges below if touch feels offset or mirrored.
    // const int16_t x = (int16_t)map(rawX, 0, 240, 0, 320);
    // const int16_t y = (int16_t)map(rawY, 0, 320, 0, 240);


    // FT6206 native portrait resolution: 240 wide × 320 tall.
    // In landscape (rotation=3), both axes are inverted relative to screen.
    //
    // Confirmed by hardware measurement:
    //   raw (  0,   0) → screen top-right    (320, 240)
    //   raw (239, 319) → screen bottom-left  (  0,   0)
    //
    // Fix: reverse both map ranges so raw max → screen 0, raw 0 → screen max.
    const int16_t x = (int16_t)map(rawX, 0, 320, 0, 240);
    const int16_t y = (int16_t)map(rawY, 240  , 0, 0,320);

    //     const int16_t x = (int16_t)map(rawX, 219, 40, 0, 240);
    // const int16_t y = (int16_t)map(rawY, 311  , 5, 0,320);
    // top right is top left



    return Point(x, y);

}
