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
{
    // Resistive touch: XPT2046 needs CS/IRQ pins passed at construction;
    // capacitive FT6206 uses I2C so nothing is needed here.
}

// ============================================================================
// begin() — hardware initialisation
// ============================================================================

bool TouchInput::begin() {
#ifdef USE_CAPACITIVE_TOUCH
    if (!_touchController.begin(40)) {
        Serial.println("TouchInput: FT6206 not found");
        return false;
    }
    Serial.println("TouchInput: FT6206 ready");
    return true;
#endif

#ifdef USE_RESISTIVE_TOUCH
    _touchController.begin();
    _touchController.setRotation(1);   // Match ILI9341 landscape rotation
    Serial.println("TouchInput: XPT2046 ready");
    return true;
#endif

    return false;  // No touch controller selected
}

// ============================================================================
// update() — poll hardware, maintain state, fire gesture detection
// ============================================================================

void TouchInput::update() {
    bool nowTouched = false;
    int16_t rawX = 0, rawY = 0;

#ifdef USE_CAPACITIVE_TOUCH
    if (_touchController.touched()) {
        TS_Point p = _touchController.getPoint();
        rawX = p.x;
        rawY = p.y;
        nowTouched = true;
    }
#endif

#ifdef USE_RESISTIVE_TOUCH
    if (_touchController.touched()) {
        TS_Point p = _touchController.getPoint();
        rawX = p.x;
        rawY = p.y;
        nowTouched = true;
    }
#endif

    if (nowTouched) {
        _currentPoint = mapCoordinates(rawX, rawY);

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
#ifdef USE_CAPACITIVE_TOUCH
    // FT6206 native resolution is typically 240×320 (portrait).
    // In landscape mode the axes are swapped: raw X → screen Y, raw Y → screen X.
    // Adjust the input ranges below if touch feels offset or mirrored.
    const int16_t x = (int16_t)map(rawX, 0, 240, 0, 320);
    const int16_t y = (int16_t)map(rawY, 0, 320, 0, 240);

    // If touch is mirrored horizontally: return Point(319 - x, y);
    // If mirrored vertically:            return Point(x, 239 - y);
    return Point(x, y);
#endif

#ifdef USE_RESISTIVE_TOUCH
    // XPT2046 ADC values — calibrate min/max from your specific panel.
    const int16_t x = (int16_t)map(rawX, 300, 3800, 0, 320);
    const int16_t y = (int16_t)map(rawY, 300, 3800, 0, 240);
    return Point(x, y);
#endif

    return Point(0, 0);
}
