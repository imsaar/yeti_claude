#include <Wire.h>
#include <Arduino.h>
#include "display.h"

// ─── Mouth position (below the eyes) ─────────────────────────────────────────
#define MOUTH_X   (OLED_W / 2)   // 64 — horizontally centred
#define MOUTH_Y   52             // below eyes (eye bottom ≈ 43)
#define MOUTH_W   12             // half-width of smile/frown arc

// ─── Face Data Table ─────────────────────────────────────────────────────────
//  EyeParams  { top_lid, bot_lid, px, py, special }
//  FaceParams { left, right, brow, mouth }
//
//  brow:  -10 = angry (inner corner DOWN)
//          0  = no brow drawn
//         +10 = sad   (inner corner UP)
//
//  mouth:  0  = none
//          1–15 = smile  (higher = deeper curve)
//         -1–-15 = frown
//         20  = surprised O
//
const FaceParams FACE_DATA[EXPR_COUNT] = {
    // EXPR_HAPPY  – squinted eyes, big smile
    { {5, 0,  0,  3, 0}, {5, 0,  0,  3, 0},  0,  10 },

    // EXPR_NEUTRAL – slight top lid, flat mouth
    { {3, 0,  0,  0, 0}, {3, 0,  0,  0, 0},  0,   1 },

    // EXPR_SAD – raised lower lid, pupils up, sad brows, frown
    { {0, 8,  0, -4, 0}, {0, 8,  0, -4, 0},  9,  -9 },

    // EXPR_SURPRISED – wide-open eyes, pupils centred, O-mouth
    { {0, 0,  0,  0, 0}, {0, 0,  0,  0, 0},  0,  20 },

    // EXPR_LOVE – heart eyes, small smile
    { {0, 0,  0,  0, 1}, {0, 0,  0,  0, 1},  0,   7 },

    // EXPR_SLEEPY – half-closed, pupils far down, slight frown
    { {13,0,  0,  6, 0}, {13,0,  0,  6, 0},  0,  -4 },

    // EXPR_ANGRY – heavy top lid, angry brows, inward pupils, flat mouth
    { {8, 0, -3,  0, 0}, {8, 0,  3,  0, 0}, -9,  -5 },

    // EXPR_DEAD – X eyes, no mouth
    { {0, 0,  0,  0, 2}, {0, 0,  0,  0, 2},  0,   0 },

    // EXPR_BLINK  (transition mid-frame only)
    { {26,0,  0,  0, 0}, {26,0,  0,  0, 0},  0,   0 },

    // EXPR_WINK_L – left closed, right squinted, slight smile
    { {26,0,  0,  0, 0}, {5, 0,  0,  0, 0},  0,   7 },

    // EXPR_WINK_R – left squinted, right closed, slight smile
    { {5, 0,  0,  0, 0}, {26,0,  0,  0, 0},  0,   7 },
};

// ─── Constructor / begin ─────────────────────────────────────────────────────
DisplayManager::DisplayManager()
    : _disp(OLED_W, OLED_H, &Wire, -1),
      _current(EXPR_HAPPY), _target(EXPR_HAPPY),
      _animating(false), _animStart(0),
      _idleBlinking(false), _idleBlinkStart(0),
      _lastBlink(0), _lastPupilMove(0),
      _idlePx(0), _idlePy(0), _targetPx(0), _targetPy(0),
      _rng(12345) {}

bool DisplayManager::begin() {
    Wire.begin(OLED_SDA, OLED_SCL);
    Wire.setClock(400000);
    if (!_disp.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
        return false;
    }
    _disp.setRotation(0);
    _disp.clearDisplay();
    _disp.display();
    return true;
}

// ─── Expression Control ───────────────────────────────────────────────────────
void DisplayManager::setExpression(Expression expr) {
    _current    = expr;
    _target     = expr;
    _animating  = false;
    _idleBlinking = false;
    _idlePx = _idlePy = _targetPx = _targetPy = 0;
    drawFaceFrame(FACE_DATA[_current]);
}

void DisplayManager::transitionTo(Expression expr) {
    if (expr == _current && !_animating) return;
    _target    = expr;
    _animating = true;
    _animStart = millis();
    _idlePx = _idlePy = 0;
    _idleBlinking = false;
}

// ─── Main update (call every loop) ───────────────────────────────────────────
void DisplayManager::update() {
    if (_animating) {
        updateAnimation();
    } else {
        doIdleAnimations();
    }
}

// ─── Transition animation ─────────────────────────────────────────────────────
void DisplayManager::updateAnimation() {
    uint32_t elapsed = millis() - _animStart;
    if (elapsed >= ANIM_DURATION_MS) {
        _current   = _target;
        _animating = false;
        drawFaceFrame(FACE_DATA[_current]);
        return;
    }

    float t = (float)elapsed / ANIM_DURATION_MS;
    FaceParams frame;
    if (t < 0.5f) {
        frame = lerpFace(FACE_DATA[_current], FACE_DATA[EXPR_BLINK], t * 2.0f);
    } else {
        frame = lerpFace(FACE_DATA[EXPR_BLINK], FACE_DATA[_target], (t - 0.5f) * 2.0f);
    }
    drawFaceFrame(frame);
}

// ─── Idle micro-animations ────────────────────────────────────────────────────
void DisplayManager::doIdleAnimations() {
    uint32_t now = millis();
    bool redraw = false;

    // Non-blocking blink: set flag, let update loop handle it
    if (!_idleBlinking && (now - _lastBlink > BLINK_INTERVAL_MS)) {
        _lastBlink     = now;
        _idleBlinking  = true;
        _idleBlinkStart = now;
        drawFaceFrame(FACE_DATA[EXPR_BLINK], 0, 0);
        return;
    }

    if (_idleBlinking) {
        if (now - _idleBlinkStart > 70) {
            _idleBlinking = false;
            redraw = true;
        } else {
            return;  // keep showing closed eyes
        }
    }

    // Occasional pupil wander
    if (now - _lastPupilMove > PUPIL_MOVE_INTERVAL_MS) {
        _lastPupilMove = now;
        uint32_t r = randNext();
        int8_t maxOff = 3;
        _targetPx = (int8_t)((r & 0xF) % (2 * maxOff + 1)) - maxOff;
        _targetPy = (int8_t)(((r >> 4) & 0xF) % (2 * maxOff + 1)) - maxOff;
        redraw = true;
    }

    // Smooth pupil slide toward target
    if (_idlePx != _targetPx || _idlePy != _targetPy) {
        if (_idlePx < _targetPx) _idlePx++;
        else if (_idlePx > _targetPx) _idlePx--;
        if (_idlePy < _targetPy) _idlePy++;
        else if (_idlePy > _targetPy) _idlePy--;
        redraw = true;
    }

    if (redraw) {
        drawFaceFrame(FACE_DATA[_current], _idlePx, _idlePy);
    }
}

// ─── Core face-drawing ────────────────────────────────────────────────────────
void DisplayManager::drawFaceFrame(const FaceParams& f, int8_t extraPx, int8_t extraPy) {
    _disp.clearDisplay();
    drawEye(L_EYE_X, L_EYE_Y, f.left,  extraPx, extraPy);
    drawEye(R_EYE_X, R_EYE_Y, f.right, extraPx, extraPy);
    if (f.brow != 0) {
        drawBrow(L_EYE_X, L_EYE_Y, f.brow, true);
        drawBrow(R_EYE_X, R_EYE_Y, f.brow, false);
    }
    if (f.mouth != 0) {
        drawMouth(MOUTH_X, MOUTH_Y, f.mouth);
    }
    _disp.display();
}

// ─── Eye ─────────────────────────────────────────────────────────────────────
void DisplayManager::drawEye(int cx, int cy, const EyeParams& e,
                              int8_t extraPx, int8_t extraPy) {
    if (e.special == 1) { drawHeart(cx, cy); return; }
    if (e.special == 2) { drawXEye(cx, cy);  return; }

    int r = EYE_R;

    // White filled circle = the eye whites
    _disp.fillCircle(cx, cy, r, SSD1306_WHITE);

    // Top eyelid — black rect from top of eye downward
    if (e.top_lid > 0) {
        _disp.fillRect(cx - r - 1, cy - r - 1, 2 * r + 3, (int)e.top_lid + 1, SSD1306_BLACK);
    }

    // Bottom eyelid — black rect from bottom of eye upward
    if (e.bot_lid > 0) {
        _disp.fillRect(cx - r - 1, cy + r - (int)e.bot_lid + 1,
                       2 * r + 3, (int)e.bot_lid + 2, SSD1306_BLACK);
    }

    // If nearly closed, replace with a single white line
    int visible = 2 * r - (int)e.top_lid - (int)e.bot_lid;
    if (visible <= 2) {
        _disp.drawFastHLine(cx - r, cy, 2 * r + 1, SSD1306_WHITE);
        return;
    }

    // Pupil — black circle, clamped inside eye
    int px = cx + e.px + extraPx;
    int py = cy + e.py + extraPy;
    int maxD = r - PUPIL_R - 2;
    px = constrain(px, cx - maxD, cx + maxD);
    py = constrain(py,
                   cy - maxD + (int)e.top_lid / 2,
                   cy + maxD - (int)e.bot_lid / 2);
    _disp.fillCircle(px, py, PUPIL_R, SSD1306_BLACK);

    // Tiny specular highlight dot
    _disp.drawPixel(px + 2, py - 2, SSD1306_WHITE);
}

// ─── Heart eye ───────────────────────────────────────────────────────────────
void DisplayManager::drawHeart(int cx, int cy) {
    // Two small circles at the top + downward triangle = heart
    _disp.fillCircle(cx - 4, cy - 3, 5, SSD1306_WHITE);
    _disp.fillCircle(cx + 4, cy - 3, 5, SSD1306_WHITE);
    _disp.fillTriangle(cx - 9, cy, cx + 9, cy, cx, cy + 9, SSD1306_WHITE);
}

// ─── Dead / dizzy X eye ──────────────────────────────────────────────────────
void DisplayManager::drawXEye(int cx, int cy) {
    int r = EYE_R - 2;
    for (int d = -1; d <= 1; d++) {
        _disp.drawLine(cx - r + d, cy - r, cx + r + d, cy + r, SSD1306_WHITE);
        _disp.drawLine(cx - r,     cy - r + d, cx + r, cy + r + d, SSD1306_WHITE);
        _disp.drawLine(cx + r + d, cy - r, cx - r + d, cy + r, SSD1306_WHITE);
        _disp.drawLine(cx + r,     cy - r + d, cx - r, cy + r + d, SSD1306_WHITE);
    }
}

// ─── Eyebrow ─────────────────────────────────────────────────────────────────
// brow < 0 = ANGRY: inner corner DOWN, outer corner UP
// brow > 0 = SAD:   inner corner UP,   outer corner DOWN
void DisplayManager::drawBrow(int cx, int cy, int8_t brow, bool isLeft) {
    int y0       = cy - EYE_R - 5;   // base brow Y, just above the eye
    int tilt_pix = (abs(brow) * 5) / 10;  // 0–5 px tilt for value 0–10

    // For the LEFT eye:  outer = left side (cx - EYE_R),  inner = right side (cx + EYE_R)
    // For the RIGHT eye: outer = right side (cx + EYE_R), inner = left side (cx - EYE_R)
    int x_outer = isLeft ? cx - EYE_R : cx + EYE_R;
    int x_inner = isLeft ? cx + EYE_R : cx - EYE_R;

    int y_outer, y_inner;
    if (brow < 0) {
        // Angry: inner DOWN (larger y), outer UP (smaller y)
        y_outer = y0 - tilt_pix;
        y_inner = y0 + tilt_pix;
    } else {
        // Sad: inner UP (smaller y), outer DOWN (larger y)
        y_outer = y0 + tilt_pix;
        y_inner = y0 - tilt_pix;
    }

    // Draw 2-pixel-thick brow
    _disp.drawLine(x_outer, y_outer,     x_inner, y_inner,     SSD1306_WHITE);
    _disp.drawLine(x_outer, y_outer + 1, x_inner, y_inner + 1, SSD1306_WHITE);
}

// ─── Mouth ───────────────────────────────────────────────────────────────────
// Uses a 4-segment polyline to approximate a parabolic smile / frown.
// mouth > 0 = smile (curves downward), mouth < 0 = frown (curves upward)
// mouth == 20 = surprised open circle
void DisplayManager::drawMouth(int cx, int cy, int8_t mouth) {
    if (mouth == 0) return;

    if (mouth >= 20) {
        // Surprised O
        _disp.drawCircle(cx, cy, 5, SSD1306_WHITE);
        _disp.drawCircle(cx, cy, 6, SSD1306_WHITE);  // 2px thick
        return;
    }

    int  w    = MOUTH_W;                            // half-width
    int  sign = (mouth > 0) ? 1 : -1;              // +1=smile, -1=frown
    int  peak = (abs(mouth) * 7) / 15;             // max ≈7 px at val 15

    // Endpoints (corners of mouth)
    int ex = w, ey = 0;
    // Quarter-point: 50% of the way along, curve is (peak/2) deep
    int qx = w / 2, qy = sign * peak / 2;
    // Centre: deepest point of curve
    int  cy_mid = sign * peak;

    // Left half: outer → quarter → centre
    _disp.drawLine(cx - ex, cy + ey,   cx - qx, cy + qy,    SSD1306_WHITE);
    _disp.drawLine(cx - qx, cy + qy,   cx,      cy + cy_mid, SSD1306_WHITE);
    // Right half: centre → quarter → outer (mirror)
    _disp.drawLine(cx,      cy + cy_mid, cx + qx, cy + qy,   SSD1306_WHITE);
    _disp.drawLine(cx + qx, cy + qy,   cx + ex, cy + ey,    SSD1306_WHITE);

    // Second pass offset by 1 px for thickness
    _disp.drawLine(cx - ex, cy + ey + sign,   cx - qx, cy + qy + sign,    SSD1306_WHITE);
    _disp.drawLine(cx - qx, cy + qy + sign,   cx,      cy + cy_mid + sign, SSD1306_WHITE);
    _disp.drawLine(cx,      cy + cy_mid + sign, cx + qx, cy + qy + sign,   SSD1306_WHITE);
    _disp.drawLine(cx + qx, cy + qy + sign,   cx + ex, cy + ey + sign,    SSD1306_WHITE);
}

// ─── Lerp helpers ─────────────────────────────────────────────────────────────
static inline uint8_t lerpU8(uint8_t a, uint8_t b, float t) {
    return (uint8_t)(a + (int)((b - a) * t));
}
static inline int8_t lerpS8(int8_t a, int8_t b, float t) {
    return (int8_t)(a + (int)((b - a) * t));
}

EyeParams DisplayManager::lerpEye(const EyeParams& a, const EyeParams& b, float t) {
    EyeParams r;
    r.top_lid = lerpU8(a.top_lid, b.top_lid, t);
    r.bot_lid = lerpU8(a.bot_lid, b.bot_lid, t);
    r.px      = lerpS8(a.px, b.px, t);
    r.py      = lerpS8(a.py, b.py, t);
    r.special = (t < 0.5f) ? a.special : b.special;
    return r;
}

FaceParams DisplayManager::lerpFace(const FaceParams& a, const FaceParams& b, float t) {
    FaceParams r;
    r.left  = lerpEye(a.left,  b.left,  t);
    r.right = lerpEye(a.right, b.right, t);
    r.brow  = lerpS8(a.brow,  b.brow,  t);
    r.mouth = lerpS8(a.mouth, b.mouth, t);
    return r;
}

// ─── Info screens ─────────────────────────────────────────────────────────────
void DisplayManager::showBootScreen() {
    _disp.clearDisplay();
    _disp.setTextColor(SSD1306_WHITE);

    // Quick happy face
    _disp.fillCircle(45, 24, 11, SSD1306_WHITE);
    _disp.fillCircle(83, 24, 11, SSD1306_WHITE);
    _disp.fillCircle(45, 24,  4, SSD1306_BLACK);
    _disp.fillCircle(83, 24,  4, SSD1306_BLACK);
    // Smile
    drawMouth(OLED_W / 2, 43, 10);

    centreText("YETI", 50, 1);
    _disp.display();
    delay(1500);
}

void DisplayManager::showSetupScreen(const char* ssid, const char* ip) {
    _disp.clearDisplay();
    _disp.setTextColor(SSD1306_WHITE);
    centreText("WiFi Setup", 0, 1);
    _disp.drawFastHLine(0, 10, OLED_W, SSD1306_WHITE);

    _disp.setTextSize(1);
    _disp.setCursor(0, 14);  _disp.print("Connect to WiFi:");
    _disp.setCursor(0, 24);  _disp.print(ssid);
    _disp.setCursor(0, 36);  _disp.print("Then browse to:");
    _disp.setCursor(0, 46);  _disp.print(ip);
    _disp.display();
}

void DisplayManager::showSleepScreen() {
    _disp.clearDisplay();
    // Closed eyes (thin lines)
    _disp.drawFastHLine(L_EYE_X - EYE_R, L_EYE_Y, EYE_R * 2, SSD1306_WHITE);
    _disp.drawFastHLine(R_EYE_X - EYE_R, R_EYE_Y, EYE_R * 2, SSD1306_WHITE);
    // Floating z's
    _disp.setTextSize(1);
    _disp.setTextColor(SSD1306_WHITE);
    _disp.setCursor(104, 12); _disp.print("z");
    _disp.setCursor(112,  5); _disp.print("z");
    _disp.display();
}

void DisplayManager::showInfoClock(const char* timeStr, const char* dateStr,
                                   float tempC, const char* weatherDesc) {
    _disp.clearDisplay();
    _disp.setTextColor(SSD1306_WHITE);

    // Big time centred
    _disp.setTextSize(3);
    int16_t x1, y1; uint16_t w, h;
    _disp.getTextBounds(timeStr, 0, 0, &x1, &y1, &w, &h);
    _disp.setCursor((OLED_W - w) / 2, 2);
    _disp.print(timeStr);

    _disp.drawFastHLine(0, 28, OLED_W, SSD1306_WHITE);

    _disp.setTextSize(1);
    _disp.getTextBounds(dateStr, 0, 0, &x1, &y1, &w, &h);
    _disp.setCursor((OLED_W - w) / 2, 32);
    _disp.print(dateStr);

    char weatherLine[32];
    if (tempC > -99.0f) {
        snprintf(weatherLine, sizeof(weatherLine), "%d%cC  %s", (int)(tempC + 0.5f), '\xB0', weatherDesc);
    } else {
        snprintf(weatherLine, sizeof(weatherLine), "--   %s", weatherDesc);
    }
    _disp.getTextBounds(weatherLine, 0, 0, &x1, &y1, &w, &h);
    _disp.setCursor((OLED_W - w) / 2, 44);
    _disp.print(weatherLine);

    _disp.display();
}

void DisplayManager::showInfoNetwork(bool wifiOk, const char* ip, int rssi, bool apMode) {
    _disp.clearDisplay();
    _disp.setTextColor(SSD1306_WHITE);
    centreText("Network", 0, 1);
    _disp.drawFastHLine(0, 10, OLED_W, SSD1306_WHITE);

    _disp.setTextSize(1);
    _disp.setCursor(0, 14);

    if (apMode) {
        _disp.print("Mode: AP Setup");
        _disp.setCursor(0, 24); _disp.print("SSID: YETI Setup");
        _disp.setCursor(0, 34); _disp.print("IP: 192.168.4.1");
    } else if (wifiOk) {
        _disp.print("WiFi: Connected");
        _disp.setCursor(0, 24); _disp.print("IP: "); _disp.print(ip);
        _disp.setCursor(0, 34); _disp.print("RSSI: "); _disp.print(rssi); _disp.print(" dBm");
        _disp.setCursor(0, 44); _disp.print("http://yeti.local");
    } else {
        _disp.print("WiFi: Offline");
        _disp.setCursor(0, 24); _disp.print("Hold 3s on this");
        _disp.setCursor(0, 34); _disp.print("screen to setup");
    }
    _disp.display();
}

void DisplayManager::showInfoFirmware(uint32_t uptimeSec) {
    _disp.clearDisplay();
    _disp.setTextColor(SSD1306_WHITE);
    centreText("Firmware", 0, 1);
    _disp.drawFastHLine(0, 10, OLED_W, SSD1306_WHITE);

    _disp.setTextSize(1);
    _disp.setCursor(0, 14); _disp.print("Version: "); _disp.print(FW_VERSION);
    _disp.setCursor(0, 24); _disp.print("Board: ESP32-C3");

    uint32_t h = uptimeSec / 3600;
    uint32_t m = (uptimeSec % 3600) / 60;
    uint32_t s = uptimeSec % 60;
    char uptime[24];
    snprintf(uptime, sizeof(uptime), "Up: %uh %02um %02us", (unsigned)h, (unsigned)m, (unsigned)s);
    _disp.setCursor(0, 34); _disp.print(uptime);
    _disp.setCursor(0, 44); _disp.print("Touch: GPIO"); _disp.print(TOUCH_PIN);
    _disp.display();
}

// ─── Utility ──────────────────────────────────────────────────────────────────
void DisplayManager::centreText(const char* str, int y, uint8_t size) {
    _disp.setTextSize(size);
    _disp.setTextColor(SSD1306_WHITE);
    int16_t x1, y1; uint16_t w, h;
    _disp.getTextBounds(str, 0, 0, &x1, &y1, &w, &h);
    _disp.setCursor((OLED_W - w) / 2, y);
    _disp.print(str);
}

uint32_t DisplayManager::randNext() {
    _rng ^= _rng << 13;
    _rng ^= _rng >> 17;
    _rng ^= _rng << 5;
    return _rng;
}
