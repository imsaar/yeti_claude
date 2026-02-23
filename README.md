# YETI — Expressive Interactive Robot Companion

A 3D-printed desk companion robot driven by an ESP32-C3 Mini. YETI displays animated facial expressions on a small OLED, reacts to capacitive touch gestures, shows live weather and clock data, and is configured entirely through a local web interface — no app or cloud account required.

The enclosure is the [Compagnon 309 by Leroyd](https://makerworld.com) on MakerWorld.

---

## Features

- **Animated face** — 11 expressions (happy, sad, angry, surprised, love, sleepy, dead, wink, and more) rendered geometrically with smooth blink transitions
- **Touch gestures** — single tap cycles expressions, double tap opens info screens, long press triggers a love expression
- **Live weather** — fetches current temperature and conditions from [Open-Meteo](https://open-meteo.com) (free, no API key)
- **NTP clock** — syncs time on boot, configurable UTC offset
- **Local web UI** — configure WiFi, location, and timezone at `http://yeti.local`
- **Offline mode** — full gesture and expression support even without WiFi
- **Sleep mode** — blanks display after 5 minutes idle, wakes on touch

---

## Hardware

### Bill of Materials

| Component | Spec | Notes |
|---|---|---|
| Microcontroller | ESP32-C3 Super Mini / Mini | 160 MHz RISC-V, 320 KB RAM, 4 MB Flash |
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

> **Note:** The touch sensor input is configured with `INPUT_PULLDOWN` to minimize noise and prevent phantom triggers. If the sensor is unresponsive, swap `TOUCH_PIN` in `config.h` to GPIO 5. Some TTP223 modules only work on GPIO 5 with certain boards.

### 3D Print Settings

- **Model:** Compagnon 309 by Leroyd (MakerWorld)
- **Layer height:** 0.2 mm, 2 walls, 15% infill
- **OLED slot:** 26 × 26 mm
- **Touch sensor slot:** 18 × 16 mm (pins face inward)
- **Magnet slots:** 8 × 2 mm in each foot (optional)
- Only the rear panel needs reprinting if ESP32-C3 position changes

---

## Software

### Prerequisites

- [PlatformIO CLI](https://docs.platformio.org/en/latest/core/installation/index.html) installed
- USB-C cable connected to the ESP32-C3

### Build & Flash

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

### Dependencies

| Library | Version |
|---|---|
| Adafruit SSD1306 | ^2.5.9 |
| Adafruit GFX Library | ^1.11.10 |
| ArduinoJson | ^7.2.0 |

---

## Setup

### First Boot

On first boot (no WiFi credentials stored), YETI starts an open access point:

1. **SSID:** `YETI Setup` — connect from any phone or laptop
2. Open a browser to `http://192.168.4.1`
3. Fill in WiFi SSID, password, latitude, longitude, and UTC offset
4. Click **Save** — YETI writes the config to non-volatile storage and reboots

### Normal Operation

After setup, YETI connects to your WiFi automatically on every boot and is reachable at `http://yeti.local`.

To re-enter setup mode from the device: double-tap to open info screens → single-tap to reach the Network screen → long-press for 3 seconds.

---

## Touch Gestures

| Gesture | Face Mode | Info Mode |
|---|---|---|
| Single tap | Advance to next expression | Advance to next info screen |
| Double tap | Open info screens | Return to face mode |
| Long press (3 s) | Show love expression for 3 s | Restart in AP setup mode (Network screen only) |

---

## Expressions

| Expression | Description | In auto-cycle |
|---|---|---|
| Happy | Slight top lid, pupils looking down | Yes |
| Neutral | Slight top lid, centred pupils | Yes |
| Sad | Raised bottom lid, angled brows | Yes |
| Surprised | Fully open eyes | Yes |
| Love | Eyes replaced with hearts | Yes |
| Sleepy | Top lid 50% closed, pupils down | Yes |
| Angry | Top lid 25%, inward brows | Yes |
| Dead | Eyes replaced with X shapes | Yes |
| Blink | Fully closed (transition only) | No |
| Wink L / Wink R | One eye closed | No |

Idle animations run continuously: eyes blink every ~3.5 s, pupils wander every ~8 s. Expressions auto-cycle every 2 minutes.

---

## Info Screens

Double-tap from face mode to enter info mode. Single-tap to cycle through three screens.

**Screen 1 — Clock & Weather**
Displays time and date synced via NTP, alongside current temperature and weather icons.

```
         14:35
       Sat 22 Feb
     22C    [ ICON ]
```
- **Large Temp:** Rounded Celsius temperature displayed in the bottom-left.
- **Weather Icons:** Custom-drawn icons for Clear (Sun), Cloudy, Rain, Snow, Storm, and Fog.
- **NTP Sync:** Robust time synchronization with fallback servers.


**Screen 2 — Network**
```
Network
────────────────────────────
WiFi: Connected
IP: 192.168.1.42
RSSI: -54 dBm
http://yeti.local
```

**Screen 3 — Firmware**
```
Firmware
────────────────────────────
Version: 1.0.0
Board: ESP32-C3
Up: 2h 15m 03s
Touch: GPIO7
```

Info mode auto-exits to face mode after 30 seconds of no interaction.

---

## Web API

| Route | Method | Description |
|---|---|---|
| `/` | GET | Config page (pre-filled from current settings) |
| `/save` | POST | Save settings to NVS and reboot |
| `/api/status` | GET | JSON with WiFi status, IP, RSSI, temp, weather, time, and stored coordinates |

---

## State Machine

```
                ┌──────────┐
   power-on ──▶ │   BOOT   │ ──▶ show logo 1.5 s
                └──────────┘
                     │
          ┌──────────┴──────────────┐
          │ no WiFi creds           │ creds stored
          ▼                         ▼
   ┌────────────┐            ┌──────────────┐
   │  SETUP_AP  │            │     FACE     │ ◀──── default state
   │192.168.4.1 │            │  (face mode) │
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

## Project Structure

```
src/
├── main.cpp      — State machine, loop(), touch dispatch
├── config.h      — Pin numbers, timing constants, enums
├── display.h/.cpp — DisplayManager: face rendering, animations
├── touch.h/.cpp  — TouchHandler: gesture classification
└── network.h/.cpp — NetworkManager: WiFi, NTP, weather, web server
platformio.ini    — Build configuration
```

---

## Platform Notes

**Apple Silicon:** `platformio.ini` pins `espressif/toolchain-riscv32-esp@12.2.0+20230208` — the only ARM64-native RISC-V toolchain version compatible with `framework-arduinoespressif32@3.20017`. The `-march=rv32imc_zicsr_zifencei` build flag is required.

**No float printf:** The Arduino ESP32 framework ships with `CONFIG_NEWLIB_NANO_FORMAT=y`, which removes `%f`/`%e`/`%g` support from printf. Using float format specifiers causes a runtime crash (Guru Meditation / Illegal instruction). Always convert floats to integers before formatting.

**32-bit Time Compliance:** Although `time_t` is 64-bit on newer ESP32 cores, YETI uses `uint32_t` for internal time storage and logic where appropriate to ensure compatibility with 32-bit architectural constraints and standard C-time casting.
