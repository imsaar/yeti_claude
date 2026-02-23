#include <Arduino.h>
#include "touch.h"

void TouchHandler::begin() {
    pinMode(TOUCH_PIN, INPUT_PULLDOWN);
    _wasPressed    = false;
    _pressStart    = 0;
    _lastReleaseMs = 0;
    _tapCount      = 0;
    _longFired     = false;
    _pendingSingle = false;
    _pendingStart  = 0;
}

// Returns true if the touch sensor is currently active (HIGH)
bool TouchHandler::rawPressed() {
    // Debounce: read twice with a short delay
    if (digitalRead(TOUCH_PIN) == HIGH) {
        delay(DEBOUNCE_MS);
        return (digitalRead(TOUCH_PIN) == HIGH);
    }
    return false;
}

TouchEvent TouchHandler::poll() {
    uint32_t now       = millis();
    bool     pressed   = rawPressed();
    TouchEvent result  = TOUCH_NONE;

    // ── Detect press START ─────────────────────────────────────────────────
    if (pressed && !_wasPressed) {
        _wasPressed  = true;
        _pressStart  = now;
        _longFired   = false;
    }

    // ── While held: fire long-press if threshold reached ───────────────────
    if (pressed && _wasPressed && !_longFired) {
        if (now - _pressStart >= LONG_PRESS_MS) {
            _longFired     = true;
            _tapCount      = 0;
            _pendingSingle = false;
            result = TOUCH_LONG;
        }
    }

    // ── Detect press END (release) ─────────────────────────────────────────
    if (!pressed && _wasPressed) {
        _wasPressed = false;
        uint32_t duration = now - _pressStart;

        if (!_longFired && duration >= DEBOUNCE_MS) {
            // Valid short tap
            _tapCount++;
            _lastReleaseMs = now;

            if (_tapCount == 1) {
                // Park the single-tap — might become a double-tap
                _pendingSingle = true;
                _pendingStart  = now;
            } else if (_tapCount >= 2) {
                // Confirmed double-tap
                _tapCount      = 0;
                _pendingSingle = false;
                result = TOUCH_DOUBLE;
            }
        }
        _longFired = false;
    }

    // ── Resolve pending single-tap after double-tap window expires ─────────
    if (_pendingSingle && (now - _pendingStart) > DOUBLE_TAP_WINDOW_MS) {
        _pendingSingle = false;
        _tapCount      = 0;
        result = TOUCH_SINGLE;
    }

    return result;
}
