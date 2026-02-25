#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include "config.h"

// ─────────────────────────────────────────────────────────────────────────────
// NetworkManager
//
//  Handles:
//   • Preferences-based config storage (WiFi creds, lat, lon, timezone)
//   • WiFi Station (STA) mode with mDNS → http://yeti.local
//   • Access Point (AP) mode for first-time setup
//   • NTP time sync
//   • Open-Meteo weather fetches (background, every 10 min)
//   • HTTP web server for the configuration page
// ─────────────────────────────────────────────────────────────────────────────
class NetworkManager {
public:
    NetworkManager();

    // Call once in setup()
    void begin();

    // Call every loop()
    void update();

    // Switch to AP setup mode
    void startAPMode();

    // ── Getters ─────────────────────────────────────────────────────────────
    bool        isConnected()  const;
    bool        isAPMode()     const { return _apMode; }
    const char* getLocalIP()   const { return _localIP;     }
    int         getRSSI()      const;
    const char* getTimeStr()   const { return _timeStr;     }
    const char* getDateStr()   const { return _dateStr;     }
    float       getTemperature() const { return _tempC;     }
    const char* getWeatherDesc() const { return _weatherDesc; }
    bool        useFahrenheit()  const { return _useFahrenheit; }

    // ── Simulation ──────────────────────────────────────────────────────────
    // Returns any pending simulated touch event and clears it (call each loop)
    TouchEvent   consumeSimulatedEvent();
    // Returns a pending expression set via the web UI (-1 = none)
    int8_t       consumePendingExpression();
    // Returns a pending buzz pattern set via the web UI (BUZZ_NONE = none)
    BuzzPattern  consumePendingBuzzPattern();

private:
    Preferences  _prefs;
    WebServer    _server;

    bool    _apMode         = false;
    bool    _wifiConnected  = false;
    bool    _ntpSynced      = false;
    char    _localIP[20]    = {};
    char    _timeStr[16]    = "--:--";
    char    _dateStr[32]    = "---";
    float   _tempC          = -99.0f;
    char    _weatherDesc[20]= "---";
    bool    _useFahrenheit  = true;

    // Config stored in Preferences
    String  _ssid;
    String  _pass;
    String  _lat;
    String  _lon;
    int32_t _tzOffsetSec    = 0;

    uint32_t   _lastWeatherMs   = 0;
    uint32_t   _lastTimeMs      = 0;
    TouchEvent  _simulatedEvent       = TOUCH_NONE;
    int8_t      _pendingExpression    = -1;
    BuzzPattern _pendingBuzzPattern   = BUZZ_NONE;

    void connectWiFi();
    void startSTA();
    void setupMDNS();
    void setupWebServer();
    void fetchWeather();
    void updateTime();

    // Web handlers
    void handleRoot();
    void handleSave();
    void handleApiStatus();
    void handleSimulate();
    void handleApiExpression();
    void handleApiBuzz();
    void handleStyle();
    void handleNotFound();
};
