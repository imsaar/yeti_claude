#pragma once

#include <Adafruit_SSD1306.h>
#include "config.h"

// Per-eye drawing parameters
struct EyeParams {
    uint8_t top_lid;    // pixels of top eyelid coverage  (0 = open, EYE_R*2 = closed)
    uint8_t bot_lid;    // pixels of bottom eyelid coverage
    int8_t  px;         // pupil X offset from eye centre
    int8_t  py;         // pupil Y offset from eye centre
    uint8_t special;    // 0=circle, 1=heart, 2=dead-X
};

// Full face descriptor
struct FaceParams {
    EyeParams left;
    EyeParams right;
    int8_t    brow;     // −10=angry (inner brow DOWN), 0=none, +10=sad (inner brow UP)
    int8_t    mouth;    // 0=none, 1–15=smile (bigger=wider curve), -1–-15=frown, 20=O-mouth
};

// Static face table (indexed by Expression enum)
extern const FaceParams FACE_DATA[EXPR_COUNT];

// ─────────────────────────────────────────────────────────────────────────────
class DisplayManager {
public:
    DisplayManager();
    bool begin();

    // ── Expression control ──────────────────────────────────────────────────
    void setExpression(Expression expr);        // instant, no animation
    void transitionTo(Expression expr);         // smooth blink transition

    // ── Info screens ────────────────────────────────────────────────────────
    void showBootScreen();
    void showSetupScreen(const char* ssid, const char* ip);
    void showSleepScreen();
    void showInfoClock(const char* timeStr, const char* dateStr,
                       float tempC, const char* weatherDesc);
    void showInfoNetwork(bool wifiOk, const char* ip, int rssi, bool apMode);
    void showInfoFirmware(uint32_t uptimeSec);

    // ── Called every loop() ─────────────────────────────────────────────────
    void update();

    // ── Queries ─────────────────────────────────────────────────────────────
    Expression currentExpression() const { return _current; }
    bool       isAnimating()       const { return _animating; }

private:
    Adafruit_SSD1306 _disp;

    Expression _current;
    Expression _target;
    bool       _animating;
    uint32_t   _animStart;

    // Idle micro-animation state
    bool     _idleBlinking;
    uint32_t _idleBlinkStart;
    uint32_t _lastBlink;
    uint32_t _lastPupilMove;
    int8_t   _idlePx, _idlePy;     // current idle pupil offset
    int8_t   _targetPx, _targetPy; // target idle pupil offset

    // ── Drawing primitives ──────────────────────────────────────────────────
    void drawFaceFrame(const FaceParams& f, int8_t extraPx = 0, int8_t extraPy = 0);
    void drawEye(int cx, int cy, const EyeParams& e, int8_t extraPx, int8_t extraPy);
    void drawHeart(int cx, int cy);
    void drawXEye(int cx, int cy);
    void drawBrow(int cx, int cy, int8_t brow, bool isLeft);
    void drawMouth(int cx, int cy, int8_t mouth);

    // ── Animation helpers ───────────────────────────────────────────────────
    void      updateAnimation();
    void      doIdleAnimations();
    EyeParams  lerpEye(const EyeParams& a, const EyeParams& b, float t);
    FaceParams lerpFace(const FaceParams& a, const FaceParams& b, float t);

    // ── Utility ─────────────────────────────────────────────────────────────
    void     centreText(const char* str, int y, uint8_t size = 1);
    uint32_t randNext();
    uint32_t _rng;
};
