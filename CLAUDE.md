# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build & Flash Commands

```bash
# Compile only
pio run

# Compile and upload to connected ESP32-C3
pio run --target upload

# Open serial monitor (115200 baud)
pio device monitor

# Compile + upload + monitor in one step
pio run --target upload && pio device monitor

# Clean build artifacts
pio run --target clean
```

## Hardware

| Component | Connection |
|---|---|
| ESP32-C3 Mini dev board | Host MCU |
| 0.96" SSD1306 OLED (I²C) | SDA → GPIO8, SCL → GPIO9 |
| TTP223 capacitive touch sensor | GPIO7 (fallback: GPIO5) |
| Optional piezo buzzer | GPIO5 |
| Optional vibration motor | GPIO10 |

## Known Platform Constraints

### Apple Silicon toolchain
`platformio.ini` pins `espressif/toolchain-riscv32-esp@12.2.0+20230208` — the only ARM64-native RISC-V toolchain version that is also compatible with `framework-arduinoespressif32@3.20017`. The `-march=rv32imc_zicsr_zifencei` build flag is required; without it GCC 12 emits `csrr` opcodes that the assembler rejects.

### No float printf — ever
The Arduino ESP32 framework ships with `CONFIG_NEWLIB_NANO_FORMAT=y`, which strips `%f`/`%e`/`%g` support from newlib's printf entirely. Calling `printf("%.1f", x)` or `snprintf(buf, n, "%.0f", x)` will crash at runtime with **Guru Meditation / Illegal instruction** inside `_svfprintf_r`. This applies to `Serial.printf`, `snprintf`, and any other printf-family call.

**Rule:** never use `%f`, `%.Nf`, `%e`, or `%g` in any format string. Convert floats to integers before formatting:
```cpp
// Wrong — will crash:
snprintf(buf, sizeof(buf), "%.1f°C", tempC);

// Correct:
snprintf(buf, sizeof(buf), "%d°C", (int)(tempC + 0.5f));
```
If sub-degree precision is ever needed, add `-u _printf_float` to `build_flags` in `platformio.ini` to force-link the full printf — but this costs ~10 KB of flash.

### No `%lu` for uint32_t
Use `%u` (not `%lu`) for `uint32_t` values. On ESP32 Arduino, `uint32_t` is `unsigned int`, not `unsigned long`. Cast explicitly: `snprintf(buf, n, "%u", (unsigned)myUint32)`.

## Architecture

The firmware is a cooperative single-loop state machine. There is **no RTOS** — everything runs in `loop()` and must be non-blocking. Any `delay()` call blocks touch detection and web server handling.

### State Machine (`main.cpp`)

```
STATE_BOOT → STATE_FACE      (normal operation)
           → STATE_SETUP_AP  (no WiFi credentials stored)

STATE_FACE ←→ STATE_INFO     (double-tap)
STATE_FACE  → STATE_SLEEP    (5 min idle timeout)
STATE_SLEEP → STATE_FACE     (any touch)
STATE_INFO  → STATE_SETUP_AP (long-press on network screen)
```

### Module Responsibilities

**`config.h`** — Single source of truth for all pin numbers, timing constants (`SLEEP_TIMEOUT_MS`, `LONG_PRESS_MS`, etc.), and all enum types (`AppState`, `Expression`, `InfoScreen`, `TouchEvent`). Change hardware pins or timing here only.

**`display.h/.cpp`** — `DisplayManager` owns the `Adafruit_SSD1306` instance. All face rendering is purely geometric (no bitmaps): filled circles clipped with black rectangles simulate eyelids, `drawMouth()` draws a 4-segment polyline smile/frown, triangles build hearts, crossed lines make X-eyes. The `FACE_DATA[]` table (indexed by `Expression` enum) stores `FaceParams{EyeParams left, right; int8_t brow, mouth}` for each expression. `transitionTo()` triggers a blink-in/blink-out animation via `updateAnimation()` which lerps between `FACE_DATA[_current]`, `FACE_DATA[EXPR_BLINK]`, and `FACE_DATA[_target]`. `doIdleAnimations()` handles periodic blinks (non-blocking, flag-based) and pupil wandering.

**`touch.h/.cpp`** — `TouchHandler` reads GPIO7 (active-HIGH) and classifies gestures. Single-tap is held in a "pending" state until `DOUBLE_TAP_WINDOW_MS` expires or a second tap arrives. Long-press fires once at `LONG_PRESS_MS` regardless of release. Returns `TOUCH_NONE` most iterations.

**`network.h/.cpp`** — `NetworkManager` handles everything network-related:
- Reads/writes config from ESP32 NVS via `Preferences` (namespace `"yeti"`, keys: `ssid`, `pass`, `lat`, `lon`, `tz`)
- First boot with no `ssid` → `startAPMode()` (SSID: `YETI Setup`, IP: `192.168.4.1`)
- Normal boot → `startSTA()` → mDNS at `yeti.local`
- `update()` is non-blocking: `_server.handleClient()` + second-tick time update + weather fetch gated by `WEATHER_INTERVAL_MS`
- Weather from Open-Meteo REST API (no API key), WMO weather codes mapped to short strings
- NTP via `configTime(_tzOffsetSec, 0, NTP_SERVER)` — timezone offset stored as raw seconds in NVS
- `fetchWeather()` is called immediately on first `update()` because `_lastWeatherMs` initialises to `0`

### Adding a New Expression

1. Add entry to `Expression` enum in `config.h`
2. Add a `FaceParams` row to `FACE_DATA[]` in `display.cpp` (same index order as the enum)
3. Optionally add it to `CYCLE_EXPRS[]` in `config.h` to include it in rotation

### Adding a New Info Screen

1. Add entry to `InfoScreen` enum in `config.h`, increment `INFO_COUNT`
2. Add a `case` in `refreshInfoDisplay()` in `main.cpp`
3. Add a `showInfo*()` method to `DisplayManager`

### Web Interface

The config page HTML is embedded as a `PROGMEM` string in `network.cpp` (variable `CONFIG_HTML`). The web server routes are:
- `GET /` → config page
- `POST /save` → saves all fields to NVS then calls `ESP.restart()`
- `GET /api/status` → JSON with current WiFi, weather, time, and stored config
