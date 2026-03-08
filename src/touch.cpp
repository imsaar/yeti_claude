#include <Arduino.h>
#include "touch.h"

void TouchHandler::begin() {
    pinMode(TOUCH_PIN, INPUT_PULLDOWN);
    _wasPressed    = false;
    _pressStart    = 0;
    _tapCount      = 0;
    _longFired     = false;
    _pendingSingle = false;
    _pendingStart  = 0;
}

// Returns true if the touch sensor is currently active (HIGH)
bool TouchHandler::rawPressed() {
    return (digitalRead(TOUCH_PIN) == HIGH);
}

TouchEvent TouchHandler::poll() {
    uint32_t now       = millis();
    bool     phys      = rawPressed();
    TouchEvent result  = TOUCH_NONE;

    // ── Debounce ───────────────────────────────────────────────────────────
    if (phys != _lastPhys) {
        _lastPhys = phys;
        _lastPhysChange = now;
    }

    // Only proceed if the signal has been stable for DEBOUNCE_MS
    if (now - _lastPhysChange < DEBOUNCE_MS) {
        // Still debouncing — use the last stable state for logic below
        phys = _wasPressed; 
    }

    // ── Detect press START ─────────────────────────────────────────────────
    if (phys && !_wasPressed) {
        _wasPressed  = true;
        _pressStart  = now;
        _mediumFired = false;
        _longFired   = false;
    }

    // ── While held: fire medium-press then long-press at their thresholds ──
    if (phys && _wasPressed && !_mediumFired && !_longFired) {
        if (now - _pressStart >= MEDIUM_PRESS_MS) {
            _mediumFired   = true;
            _tapCount      = 0;
            _pendingSingle = false;
            result = TOUCH_MEDIUM;
        }
    }
    if (phys && _wasPressed && !_longFired) {
        if (now - _pressStart >= LONG_PRESS_MS) {
            _longFired     = true;
            _tapCount      = 0;
            _pendingSingle = false;
            result = TOUCH_LONG;
        }
    }

    // ── Detect press END (release) ─────────────────────────────────────────
    if (!phys && _wasPressed) {
        _wasPressed = false;

        if (!_longFired && !_mediumFired) {
            // Valid short tap
            _tapCount++;
            if (_tapCount == 1) {
                _pendingSingle = true;
                _pendingStart  = now;
            } else if (_tapCount >= 2) {
                _tapCount      = 0;
                _pendingSingle = false;
                result = TOUCH_DOUBLE;
            }
        }
        _mediumFired = false;
        _longFired   = false;
    }

    // ── Resolve pending single-tap after double-tap window expires ─────────
    if (_pendingSingle && (now - _pendingStart) > DOUBLE_TAP_WINDOW_MS) {
        _pendingSingle = false;
        _tapCount      = 0;
        result = TOUCH_SINGLE;
    }

    return result;
}
