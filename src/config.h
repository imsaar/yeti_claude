#pragma once

#include <stdint.h>

// ─── Firmware ────────────────────────────────────────────────────────────────
#define FW_VERSION  "1.0.0"

// ─── Display Pins (I²C) ──────────────────────────────────────────────────────
#define OLED_SDA    8
#define OLED_SCL    9
#define OLED_ADDR   0x3C
#define OLED_W      128
#define OLED_H      64

// ─── Input Pins ──────────────────────────────────────────────────────────────
#define TOUCH_PIN   7       // TTP223 — try GPIO5 if GPIO7 doesn't work
#define BUZZER_PIN  5       // Optional piezo buzzer
#define MOTOR_PIN   10      // Optional vibration motor

// ─── Face Geometry (128×64 canvas) ───────────────────────────────────────────
#define EYE_R       13      // eye radius (pixels)
#define PUPIL_R     5       // pupil radius
#define L_EYE_X     35      // left  eye centre X
#define L_EYE_Y     30      // left  eye centre Y
#define R_EYE_X     93      // right eye centre X
#define R_EYE_Y     30      // right eye centre Y

// ─── Timing (milliseconds) ───────────────────────────────────────────────────
#define EXPRESSION_CYCLE_MS   120000UL  // auto-cycle every 2 min
#define SLEEP_TIMEOUT_MS      300000UL  // sleep after 5 min idle
#define BLINK_INTERVAL_MS      3500UL  // idle blink period
#define PUPIL_MOVE_INTERVAL_MS 8000UL  // idle pupil wander period
#define DOUBLE_TAP_WINDOW_MS    400UL  // max gap between two taps
#define LONG_PRESS_MS          3000UL  // long-press threshold
#define DEBOUNCE_MS              50UL  // touch de-bounce
#define LOVE_HOLD_MS           3000UL  // show love face before returning
#define INFO_AUTO_EXIT_MS     30000UL  // auto-exit info after 30 s
#define INFO_REFRESH_MS        1000UL  // re-draw info every second
#define WEATHER_INTERVAL_MS  600000UL  // fetch weather every 10 min
#define ANIM_DURATION_MS        350UL  // blink transition duration

// ─── WiFi / Network ──────────────────────────────────────────────────────────
#define AP_SSID     "YETI"
#define AP_PASS     "123456789"
#define HOSTNAME    "yeti"          // → http://yeti.local
#define NTP_SERVER  "pool.ntp.org"

// ─── Application State ───────────────────────────────────────────────────────
enum AppState : uint8_t {
    STATE_BOOT,
    STATE_FACE,
    STATE_INFO,
    STATE_SLEEP,
    STATE_SETUP_AP,
};

// ─── Face Expressions ────────────────────────────────────────────────────────
enum Expression : uint8_t {
    EXPR_HAPPY = 0,
    EXPR_NEUTRAL,
    EXPR_SAD,
    EXPR_SURPRISED,
    EXPR_LOVE,
    EXPR_SLEEPY,
    EXPR_ANGRY,
    EXPR_DEAD,
    EXPR_BLINK,
    EXPR_WINK_L,
    EXPR_WINK_R,
    EXPR_COUNT
};

// Subset cycled automatically / on single tap
#define CYCLE_COUNT 7
static const Expression CYCLE_EXPRS[CYCLE_COUNT] = {
    EXPR_HAPPY, EXPR_NEUTRAL, EXPR_SURPRISED,
    EXPR_SAD,   EXPR_SLEEPY,  EXPR_ANGRY, EXPR_HAPPY
};

// ─── Info Screens ────────────────────────────────────────────────────────────
enum InfoScreen : uint8_t {
    INFO_CLOCK    = 0,
    INFO_NETWORK  = 1,
    INFO_FIRMWARE = 2,
    INFO_COUNT    = 3
};

// ─── Touch Events ────────────────────────────────────────────────────────────
enum TouchEvent : uint8_t {
    TOUCH_NONE   = 0,
    TOUCH_SINGLE,
    TOUCH_DOUBLE,
    TOUCH_LONG,
};

// ─── Vibration Patterns ───────────────────────────────────────────────────────
enum VibePattern : uint8_t {
    VIBE_NONE        = 0,
    VIBE_BOOT,        // single long pulse on startup
    VIBE_TAP,         // short click on single-tap
    VIBE_DOUBLE_TAP,  // two pulses on double-tap
    VIBE_LONG_PRESS,  // strong long pulse on long-press
    VIBE_ALERT,       // three rapid pulses
};

// ─── Buzzer Patterns ─────────────────────────────────────────────────────────
enum BuzzPattern : uint8_t {
    BUZZ_NONE        = 0,
    BUZZ_BOOT,        // rising three-note chime on startup
    BUZZ_TAP,         // short click on single-tap
    BUZZ_DOUBLE_TAP,  // two-click on double-tap
    BUZZ_LONG_PRESS,  // love fanfare on long-press
    BUZZ_HAPPY,       // ascending arpeggio
    BUZZ_SAD,         // descending phrase
    BUZZ_ALERT,       // three warning beeps
    BUZZ_STARWARS,    // Star Wars main theme (two phrases)
    BUZZ_COUNT
};
