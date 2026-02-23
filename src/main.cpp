#include <Arduino.h>
#include "config.h"
#include "display.h"
#include "touch.h"
#include "network.h"

// ─── Globals ──────────────────────────────────────────────────────────────────
static DisplayManager disp;
static TouchHandler   touch;
static NetworkManager net;

static AppState   appState          = STATE_BOOT;
static InfoScreen infoScreen        = INFO_CLOCK;
static uint8_t    cycleIdx          = 0;
static uint32_t   lastInteraction   = 0;
static uint32_t   lastCycleMs       = 0;
static uint32_t   infoEnteredMs     = 0;
static uint32_t   lastInfoRefreshMs = 0;
static bool       loveReturning     = false;
static uint32_t   loveStartMs       = 0;

// ─── Forward declarations ─────────────────────────────────────────────────────
static void handleFaceState(TouchEvent evt);
static void handleInfoState(TouchEvent evt);
static void refreshInfoDisplay();
static void enterFaceMode();
static void enterInfoMode();

// ─── setup() ─────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println("\n[YETI] Booting…");

    // Display (must come first so we can show boot screen)
    if (!disp.begin()) {
        Serial.println("[YETI] OLED not found! Check wiring.");
    }
    disp.showBootScreen();

    // Touch sensor
    touch.begin();

    // Network (WiFi, NTP, web server)
    net.begin();

    // Show setup screen if in AP mode
    if (net.isAPMode()) {
        appState = STATE_SETUP_AP;
        disp.showSetupScreen(AP_SSID, "192.168.4.1");
    } else {
        enterFaceMode();
    }

    lastInteraction = millis();
    Serial.println("[YETI] Ready.");
}

// ─── loop() ──────────────────────────────────────────────────────────────────
void loop() {
    // Always service display animations and web server
    disp.update();
    net.update();

    TouchEvent evt = touch.poll();
    // Merge any event injected via the web UI simulation panel
    { TouchEvent sim = net.consumeSimulatedEvent(); if (sim != TOUCH_NONE) evt = sim; }

    // Apply any expression set directly from the web UI
    {
        int8_t pe = net.consumePendingExpression();
        if (pe >= 0) {
            Expression expr = (Expression)pe;
            disp.transitionTo(expr);
            loveReturning = false;
            lastCycleMs = millis();
            lastInteraction = millis();
            // Sync cycleIdx so single-tap continues from this expression
            for (uint8_t i = 0; i < CYCLE_COUNT; i++) {
                if (CYCLE_EXPRS[i] == expr) { cycleIdx = i; break; }
            }
            if (appState != STATE_FACE) enterFaceMode();
        }
    }

    // Any touch resets idle / wakes from sleep
    if (evt != TOUCH_NONE) {
        lastInteraction = millis();

        if (appState == STATE_SLEEP) {
            enterFaceMode();
            return;
        }
        // Also wake from AP-mode (in case user wants to go back to face mode)
        if (appState == STATE_SETUP_AP && evt == TOUCH_DOUBLE) {
            enterFaceMode();
            return;
        }
    }

    // State machine
    switch (appState) {
        case STATE_FACE:    handleFaceState(evt); break;
        case STATE_INFO:    handleInfoState(evt); break;
        case STATE_SLEEP:   break;   // waiting for touch — handled above
        case STATE_SETUP_AP: break;  // waiting for web config
        default:             break;
    }

    // Auto-sleep after inactivity
    if ((appState == STATE_FACE) &&
        (millis() - lastInteraction > SLEEP_TIMEOUT_MS)) {
        appState = STATE_SLEEP;
        disp.showSleepScreen();
        Serial.println("[YETI] Sleeping…");
    }
}

// ─── Face mode ────────────────────────────────────────────────────────────────
static void handleFaceState(TouchEvent evt) {
    // Handle returning from love expression
    if (loveReturning && (millis() - loveStartMs >= LOVE_HOLD_MS)) {
        loveReturning = false;
        disp.transitionTo(EXPR_HAPPY);
        cycleIdx = 0;   // reset cycle to happy
        lastCycleMs = millis();
    }

    switch (evt) {
        case TOUCH_SINGLE:
            loveReturning = false;
            cycleIdx = (cycleIdx + 1) % CYCLE_COUNT;
            disp.transitionTo(CYCLE_EXPRS[cycleIdx]);
            lastCycleMs = millis();
            Serial.printf("[FACE] Expression -> %d\n", CYCLE_EXPRS[cycleIdx]);
            break;

        case TOUCH_DOUBLE:
            loveReturning = false;
            enterInfoMode();
            break;

        case TOUCH_LONG:
            // Love expression + optional buzzer/motor
            loveReturning = true;
            loveStartMs   = millis();
            disp.transitionTo(EXPR_LOVE);
            Serial.println("[FACE] Love!");
            // Optional: digitalWrite(MOTOR_PIN, HIGH); delay(200); digitalWrite(MOTOR_PIN, LOW);
            break;

        default:
            break;
    }

    // Auto-cycle expressions every 2 minutes
    if (!loveReturning && (millis() - lastCycleMs >= EXPRESSION_CYCLE_MS)) {
        cycleIdx = (cycleIdx + 1) % CYCLE_COUNT;
        disp.transitionTo(CYCLE_EXPRS[cycleIdx]);
        lastCycleMs = millis();
        Serial.printf("[FACE] Auto-cycle -> %d\n", CYCLE_EXPRS[cycleIdx]);
    }
}

// ─── Info mode ────────────────────────────────────────────────────────────────
static void handleInfoState(TouchEvent evt) {
    switch (evt) {
        case TOUCH_SINGLE:
            infoScreen = (InfoScreen)((infoScreen + 1) % INFO_COUNT);
            infoEnteredMs     = millis();
            lastInfoRefreshMs = 0;  // force immediate refresh
            break;

        case TOUCH_DOUBLE:
            // Exit info → face
            enterFaceMode();
            return;

        case TOUCH_LONG:
            // On network screen, long-press triggers AP mode / re-setup
            if (infoScreen == INFO_NETWORK) {
                net.startAPMode();
                appState = STATE_SETUP_AP;
                disp.showSetupScreen(AP_SSID, "192.168.4.1");
                return;
            }
            break;

        default:
            break;
    }

    // Refresh info display periodically
    if (millis() - lastInfoRefreshMs >= INFO_REFRESH_MS) {
        lastInfoRefreshMs = millis();
        refreshInfoDisplay();
    }

    // Auto-exit info after timeout
    if (millis() - infoEnteredMs >= INFO_AUTO_EXIT_MS) {
        enterFaceMode();
    }
}

// ─── Helpers ──────────────────────────────────────────────────────────────────
static void refreshInfoDisplay() {
    switch (infoScreen) {
        case INFO_CLOCK:
            disp.showInfoClock(net.getTimeStr(), net.getDateStr(),
                               net.getTemperature(), net.getWeatherDesc());
            break;
        case INFO_NETWORK:
            disp.showInfoNetwork(net.isConnected(), net.getLocalIP(),
                                 net.getRSSI(), net.isAPMode());
            break;
        case INFO_FIRMWARE:
            disp.showInfoFirmware(millis() / 1000);
            break;
        default:
            break;
    }
}

static void enterFaceMode() {
    appState          = STATE_FACE;
    loveReturning     = false;
    lastCycleMs       = millis();
    lastInteraction   = millis();
    disp.setExpression(CYCLE_EXPRS[cycleIdx]);
    Serial.println("[YETI] Face mode");
}

static void enterInfoMode() {
    appState          = STATE_INFO;
    infoScreen        = INFO_CLOCK;
    infoEnteredMs     = millis();
    lastInfoRefreshMs = 0;
    refreshInfoDisplay();
    Serial.println("[YETI] Info mode");
}
