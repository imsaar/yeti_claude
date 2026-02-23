# YETI — Expressive Interactive Robot Companion
## Project Specification

---

## Overview

YETI is a 3D-printed desk companion robot (Compagnon 309 by Leroyd on MakerWorld) driven by an ESP32-C3 Mini. It displays animated facial expressions on a small OLED, reacts to capacitive touch gestures, shows live weather and clock data, and is configured entirely through a local web interface — no app or cloud account required.

---

## Hardware

### Bill of Materials

| Component | Spec | Notes |
|---|---|---|
| Microcontroller | ESP32-C3 Super Mini / Mini dev board | 160 MHz RISC-V, 320 KB RAM, 4 MB Flash |
| Display | 0.96" SSD1306 OLED, I²C, 128×64 px | 26 mm × 26 mm module |
| Touch sensor | TTP223 capacitive touch module | 18 × 16 mm max footprint |
| Wiring | Dupont jumper wires | ~7 cm recommended for SDA/SCL |
| Magnets (optional) | 8 × 2 mm neodymium, ×4 | For mounting in each foot slot |
| Vibration motor (optional) | Micro motor module | Fits dedicated internal slot |
| Piezo buzzer (optional) | Any 3.3V-compatible buzzer | |

### Wiring

| Signal | ESP32-C3 GPIO |
|---|---|
| OLED VCC | 3.3V |
| OLED GND | GND |
| OLED SDA | GPIO 8 |
| OLED SCL | GPIO 9 |
| Touch VCC | 3.3V |
| Touch GND | GND |
| Touch I/O | GPIO 7 (fallback: GPIO 5) |
| Buzzer (optional) | GPIO 5 |
| Vibration motor (optional) | GPIO 10 |

> **Note:** GPIO 7 is the default touch pin. Some TTP223 modules have been reported to only work on GPIO 5 on certain boards — swap `TOUCH_PIN` in `config.h` if the sensor is unresponsive.

---

## Software Stack

| Layer | Technology |
|---|---|
| Build system | PlatformIO CLI |
| Platform | `espressif32` v6.12.0 |
| Board target | `lolin_c3_mini` |
| Framework | Arduino |
| Toolchain | `espressif/toolchain-riscv32-esp@12.2.0+20230208` (ARM64-native for Apple Silicon) |
| Display driver | Adafruit SSD1306 + Adafruit GFX |
| JSON | ArduinoJson v7 |
| Config storage | ESP32 NVS via `Preferences` library |
| Web server | Arduino `WebServer` (port 80) |
| mDNS | `ESPmDNS` → `http://yeti.local` |
| Weather API | Open-Meteo (free, no key) |
| Time sync | NTP via `configTime()` |

### Key Build Flags

```ini
-DARDUINO_USB_CDC_ON_BOOT=1          ; enables USB CDC serial on ESP32-C3
-march=rv32imc_zicsr_zifencei        ; required for GCC 12 + old framework combo
```

---

## Application States

```
                ┌──────────┐
   power-on ──▶ │   BOOT   │ ──▶ show logo 1.5s
                └──────────┘
                     │
          ┌──────────┴──────────────┐
          │ no WiFi creds           │ creds stored
          ▼                         ▼
   ┌────────────┐            ┌──────────────┐
   │  SETUP_AP  │            │     FACE     │ ◀──── default state
   │ yeti.local │            │  (face mode) │
   └────────────┘            └──────────────┘
                                   │  ▲
                       double-tap  │  │ double-tap
                                   ▼  │
                             ┌──────────┐
                             │   INFO   │
                             │ 3 screens│
                             └──────────┘
                                   │
                    long-press on   │
                    network screen  ▼
                             ┌────────────┐
                             │  SETUP_AP  │
                             └────────────┘

   FACE ──▶ SLEEP   after 5 min idle
   SLEEP ──▶ FACE   on any touch
```

---

## Face Expressions

All expressions are rendered geometrically on the 128×64 canvas using Adafruit GFX primitives — no bitmap storage. Each eye is a white filled circle clipped with black rectangles to simulate eyelids.

### Eye Geometry

| Parameter | Value |
|---|---|
| Left eye centre | (35, 30) |
| Right eye centre | (93, 30) |
| Eye radius | 13 px |
| Pupil radius | 5 px |

### Expression Catalogue

| Expression | Effect | Auto-cycle? |
|---|---|---|
| `EXPR_HAPPY` | Slight top lid, pupils looking slightly down | Yes |
| `EXPR_NEUTRAL` | Slight top lid, centred pupils | Yes |
| `EXPR_SAD` | Raised bottom lid, pupils looking up, angled brows | Yes |
| `EXPR_SURPRISED` | Fully open eyes, no lid | Yes |
| `EXPR_LOVE` | Both eyes replaced with white hearts | Yes |
| `EXPR_SLEEPY` | Top lid covers 50%, pupils looking far down | Yes |
| `EXPR_ANGRY` | 25% top lid, inward-leaning brows, offset pupils | Yes |
| `EXPR_DEAD` | Both eyes replaced with white X shapes | Yes |
| `EXPR_BLINK` | Eyes fully closed (used as transition mid-frame only) | No |
| `EXPR_WINK_L` | Left eye closed, right eye normal | No |
| `EXPR_WINK_R` | Left eye normal, right eye closed | No |

### Idle Micro-Animations

- **Blink:** eyes close for 80 ms every ~3.5 seconds
- **Pupil wander:** pupils drift to a random ±3 px offset every ~8 seconds, animate smoothly toward target each `update()` call

### Expression Transitions

Calling `transitionTo(expr)` triggers a 350 ms blink animation:
- First half: lerp from current face → fully closed (EXPR_BLINK)
- Second half: lerp from fully closed → target face
- All eye parameters (top_lid, bot_lid, pupil offsets) are linearly interpolated

---

## Touch Interactions

### Gesture Detection

| Gesture | Criteria | Action |
|---|---|---|
| Single tap | Press released < 1s, no second tap within 400 ms | Advance to next expression in cycle |
| Double tap | Two taps within 400 ms window | Enter / exit info mode |
| Long press | Held ≥ 3000 ms | Show love expression; auto-return to happy after 3s |

The single-tap is held in a pending state until the double-tap window expires, ensuring no false single-taps fire on double-tap gestures.

### Face Mode Behaviour

- **Single tap** → advances `cycleIdx` through `CYCLE_EXPRS[]` with a blink transition
- **Double tap** → enters info mode at `INFO_CLOCK` screen
- **Long press** → shows `EXPR_LOVE`, then after `LOVE_HOLD_MS` (3 s) transitions back to `EXPR_HAPPY`
- **Auto-cycle** → every 2 minutes, advances to next expression automatically
- **Sleep** → after 5 minutes with no interaction, displays closed eyes + "z z" and blanks the face

### Info Mode Behaviour

- **Single tap** → advance through info screens (Clock → Network → Firmware → Clock…)
- **Double tap** → exit back to face mode
- **Long press on Network screen** → restart in AP setup mode
- **Auto-exit** → returns to face mode after 30 seconds of no interaction

---

## Info Screens

### Screen 1 — Clock & Weather

```
┌────────────────────────────┐
│         14:35              │  ← textSize 3, centred
├────────────────────────────┤
│       Sat 22 Feb           │  ← textSize 1, centred
│     22°C  Partly Cloudy    │  ← textSize 1, centred
└────────────────────────────┘
```

### Screen 2 — Network Status

Connected state:
```
Network
────────────────────────────
WiFi: Connected
IP: 192.168.1.42
RSSI: -54 dBm
http://yeti.local
```

Offline state:
```
Network
────────────────────────────
WiFi: Offline
Hold 3s on this
screen to setup
```

### Screen 3 — Firmware

```
Firmware
────────────────────────────
Version: 1.0.0
Board: ESP32-C3
Up: 2h 15m 03s
Touch: GPIO7
```

---

## WiFi & Network

### First Boot — AP Setup Mode

1. ESP32-C3 starts a soft AP: SSID **`YETI Setup`**, open (no password)
2. Display shows AP SSID and IP
3. User connects to `YETI Setup` WiFi on phone/laptop
4. User browses to `http://192.168.4.1`
5. Config page collects: WiFi SSID, password, latitude, longitude, UTC offset
6. On save: all values written to NVS, device reboots into STA mode

### Normal Operation — STA Mode

- Connects to saved WiFi on boot (15s timeout; falls back to offline mode on failure)
- mDNS hostname: `yeti` → `http://yeti.local`
- NTP sync with `pool.ntp.org`, timezone applied as raw UTC offset seconds
- Weather fetched from Open-Meteo every 10 minutes

### Web Interface Routes

| Route | Method | Description |
|---|---|---|
| `/` | GET | Serves the config HTML page (embedded PROGMEM) |
| `/save` | POST | Saves form fields to NVS, serves confirmation, then reboots |
| `/api/status` | GET | Returns JSON: WiFi status, IP, RSSI, temp, weather, time, stored coords |

Config page pre-fills all fields from `/api/status` on load via `fetch()`.

### NVS Config Keys (namespace: `"yeti"`)

| Key | Type | Default | Description |
|---|---|---|---|
| `ssid` | String | `""` | WiFi network name |
| `pass` | String | `""` | WiFi password |
| `lat` | String | `"48.8566"` | Latitude (Paris default) |
| `lon` | String | `"2.3522"` | Longitude (Paris default) |
| `tz` | Long | `0` | UTC offset in seconds |

---

## Weather API

**Endpoint:** `http://api.open-meteo.com/v1/forecast`

**Parameters:** `latitude`, `longitude`, `current=temperature_2m,weathercode`, `forecast_days=1`

**WMO Code Mapping:**

| WMO Code Range | Display String |
|---|---|
| 0 | Clear |
| 1–3 | Cloudy |
| 4–48 | Foggy |
| 49–67 | Rain |
| 68–77 | Snow |
| 78–82 | Showers |
| 83–99 | Storm |

No API key required. HTTP timeout: 8 seconds.

---

## Timing Constants

| Constant | Value | Description |
|---|---|---|
| `EXPRESSION_CYCLE_MS` | 120,000 ms | Auto-cycle interval in face mode |
| `SLEEP_TIMEOUT_MS` | 300,000 ms | Idle time before sleep |
| `BLINK_INTERVAL_MS` | 3,500 ms | Periodic idle blink |
| `PUPIL_MOVE_INTERVAL_MS` | 8,000 ms | Pupil wander trigger |
| `DOUBLE_TAP_WINDOW_MS` | 400 ms | Window for double-tap recognition |
| `LONG_PRESS_MS` | 3,000 ms | Long-press threshold |
| `DEBOUNCE_MS` | 50 ms | Touch de-bounce |
| `LOVE_HOLD_MS` | 3,000 ms | Duration of love expression |
| `INFO_AUTO_EXIT_MS` | 30,000 ms | Auto-return from info screens |
| `ANIM_DURATION_MS` | 350 ms | Blink transition total duration |
| `WEATHER_INTERVAL_MS` | 600,000 ms | Weather API refresh interval |

---

## 3D Print Reference

- **Model:** Compagnon 309 by Leroyd (MakerWorld)
- **Recommended settings:** 0.2 mm layer, 2 walls, 15% infill
- **OLED slot:** 26 × 26 mm
- **Touch sensor slot:** 18 × 16 mm (sensor pins face inward)
- **Magnet slots:** 8 × 2 mm in each foot (optional)
- **Vibration motor slot:** dedicated internal recess (optional)
- Only the rear panel needs reprinting if ESP32-C3 position changes

---

## Offline Mode

When no WiFi credentials are stored (or connection fails):
- Face mode is fully functional: expressions, auto-cycle, touch gestures
- Info screens show `--:--` for time, `---` for weather
- Network screen shows offline status and instructions to long-press for AP setup
- No weather or NTP data is fetched
