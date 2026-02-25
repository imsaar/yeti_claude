#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <time.h>
#include "network.h"

// ─── Constructor ──────────────────────────────────────────────────────────────
NetworkManager::NetworkManager() : _server(80) {}

// ─── begin() ─────────────────────────────────────────────────────────────────
void NetworkManager::begin() {
    _prefs.begin("yeti", false);
    _ssid        = _prefs.getString("ssid", "");
    _pass        = _prefs.getString("pass", "");
    _lat         = _prefs.getString("lat",  "48.8566"); _lat.trim();
    _lon         = _prefs.getString("lon",  "2.3522"); _lon.trim();
    _tzOffsetSec = _prefs.getLong("tz", 0);
    _useFahrenheit = _prefs.getBool("faren", true);

    if (_ssid.length() == 0) {
        startAPMode();
    } else {
        startSTA();
    }
    setupWebServer();
}

void NetworkManager::update() {
    _server.handleClient();
    uint32_t now = millis();
    if (!_apMode && _wifiConnected) {
        if (WiFi.status() != WL_CONNECTED) {
            Serial.println("[NET] WiFi connection lost!");
            _wifiConnected = false;
        }
        if (now - _lastTimeMs >= 1000) {
            _lastTimeMs = now;
            updateTime();
        }
        uint32_t interval = (_tempC < -90) ? 60000UL : WEATHER_INTERVAL_MS;
        if (_lastWeatherMs == 0 || now - _lastWeatherMs >= interval) {
            _lastWeatherMs = now;
            fetchWeather();
        }
    }
}

void NetworkManager::startAPMode() {
    _apMode = true;
    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_SSID, AP_PASS);
    snprintf(_localIP, sizeof(_localIP), "192.168.4.1");
    Serial.printf("[NET] AP mode: SSID=%s IP=%s\n", AP_SSID, _localIP);
}

void NetworkManager::startSTA() {
    _apMode = false;
    WiFi.mode(WIFI_STA);
    WiFi.setHostname(HOSTNAME);
    WiFi.begin(_ssid.c_str(), _pass.c_str());
    Serial.printf("[NET] Connecting to %s …\n", _ssid.c_str());
    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
        delay(300); Serial.print('.');
    }
    if (WiFi.status() == WL_CONNECTED) {
        _wifiConnected = true;
        strncpy(_localIP, WiFi.localIP().toString().c_str(), sizeof(_localIP) - 1);
        Serial.printf("\n[NET] Connected! IP: %s\n", _localIP);
        setupMDNS();
        configTime(_tzOffsetSec, 0, NTP_SERVER, "time.google.com");
    } else {
        Serial.println("\n[NET] Failed to connect → offline mode");
        _wifiConnected = false;
    }
}

void NetworkManager::setupMDNS() {
    if (MDNS.begin(HOSTNAME)) MDNS.addService("http", "tcp", 80);
}

void NetworkManager::setupWebServer() {
    _server.on("/",              [this]() { handleRoot(); });
    _server.on("/s.css",         [this]() { handleStyle(); });
    _server.on("/save",          HTTP_POST, [this]() { handleSave(); });
    _server.on("/api/status",    [this]() { handleApiStatus(); });
    _server.on("/api/simulate",  HTTP_POST, [this]() { handleSimulate(); });
    _server.on("/api/expression",HTTP_POST, [this]() { handleApiExpression(); });
    _server.on("/api/buzz",      HTTP_POST, [this]() { handleApiBuzz(); });
    _server.on("/favicon.ico",   HTTP_GET,  [this]() { _server.send(204, "text/plain", ""); });
    _server.onNotFound([this]() { handleNotFound(); });
    _server.begin();
}

void NetworkManager::handleStyle() {
    static const char css[] PROGMEM = R"css(
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:system-ui,sans-serif;background:#111;color:#eee;display:flex;justify-content:center;padding:1.5rem 1rem}
.W{background:#1e1e1e;border-radius:16px;padding:2rem;width:100%;max-width:820px;box-shadow:0 8px 32px #0008}
h1{font-size:1.6rem;text-align:center;margin-bottom:1.5rem}
h1 span{color:#6af}
label{display:block;font-size:.8rem;color:#999;margin:1rem 0 .25rem}
input,select{width:100%;padding:.55rem .75rem;background:#2a2a2a;border:1px solid #444;border-radius:8px;color:#eee;outline:none}
input:focus,select:focus{border-color:#6af}
.R{display:flex;gap:.75rem}.R>div{flex:1}
.G{display:grid;grid-template-columns:1fr 1fr;gap:0}
.C{border-left:1px solid #333;padding-left:2rem}
@media(max-width:580px){.G{grid-template-columns:1fr}.C{border-left:none;padding-top:1.5rem;margin-top:1.5rem;border-top:1px solid #333}}
button{margin-top:1.5rem;width:100%;padding:.7rem;background:#6af;border:none;border-radius:8px;color:#111;font-size:1rem;font-weight:700;cursor:pointer}
.S{margin-top:1.5rem;padding-top:1.5rem;border-top:1px solid #333}
.S0{margin-top:0}
h2{font-size:1rem;color:#aaa;margin-bottom:.5rem}
.badge{display:inline-block;background:#2a2a2a;border:1px solid #444;border-radius:6px;padding:.25rem .6rem;font-size:.8rem;margin:.15rem 0}
.SR{display:flex;gap:.5rem;margin-top:.5rem}.SR>form{flex:1}
.sb{margin-top:0;padding:.6rem .25rem;background:#2a2a2a;border:1px solid #555;color:#eee;font-size:.85rem;font-weight:600;cursor:pointer}
.FG{display:grid;grid-template-columns:repeat(5,1fr);gap:.35rem;margin-top:.5rem}
.fb{margin-top:0;padding:.45rem .1rem;font-size:.7rem;background:#2a2a2a;border:1px solid #555;color:#eee;font-weight:600;border-radius:8px;cursor:pointer}
.fb:hover,.sb:hover{background:#3a3a3a}
.ht{font-size:.75rem;color:#666;margin-top:.35rem}
)css";
    _server.sendHeader("Cache-Control", "max-age=86400");
    _server.send_P(200, "text/css", css);
}

void NetworkManager::handleRoot() {
    String html;
    html.reserve(8192);
    html += R"rawhtml(<!DOCTYPE html><html lang="en"><head><meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>YETI</title><link rel="stylesheet" href="/s.css"></head><body><div class="W"><h1>🧊 <span>YETI</span> Config</h1><div class="G"><div>
<form action="/save" method="POST"><div class="S0"><h2>WiFi</h2>)rawhtml";

    html += "<label>SSID</label><input name=\"ssid\" type=\"text\" value=\"";
    html += _ssid;
    html += "\" required>";
    html += R"rawhtml(<label>Password</label><input name="pass" type="password" placeholder="(current saved)"></div>
<div class="S"><h2>Location</h2><div class="R">)rawhtml";
    html += "<div><label>Lat</label><input name=\"lat\" type=\"text\" value=\"";
    html += _lat;
    html += "\" required></div>";
    html += "<div><label>Lon</label><input name=\"lon\" type=\"text\" value=\"";
    html += _lon;
    html += "\" required></div>";
    html += R"rawhtml(</div></div><div class="S"><h2>Timezone</h2><select name="tz">)rawhtml";

    static const int32_t tz_vals[] = {
        -43200, -39600, -36000, -32400, -28800, -25200, -21600, -18000, -14400, -10800,
        -7200, -3600, 0, 3600, 7200, 10800, 12600, 14400, 16200, 18000, 19800, 20700,
        21600, 23400, 25200, 28800, 32400, 34200, 36000, 39600, 43200
    };
    static const char* tz_labels[] = {
        "UTC-12", "UTC-11", "UTC-10", "UTC-9", "UTC-8", "UTC-7", "UTC-6", "UTC-5", "UTC-4", "UTC-3",
        "UTC-2", "UTC-1", "UTC+0", "UTC+1", "UTC+2", "UTC+3", "3:30", "UTC+4", "4:30", "UTC+5",
        "5:30", "5:45", "UTC+6", "6:30", "UTC+7", "UTC+8", "UTC+9", "9:30",
        "UTC+10", "UTC+11", "UTC+12"
    };
    for (int i = 0; i < 31; i++) {
        html += "<option value=\"";
        html += String(tz_vals[i]);
        html += "\"";
        if (tz_vals[i] == _tzOffsetSec) html += " selected";
        html += ">";
        html += tz_labels[i];
        html += "</option>";
    }
    
    html += R"rawhtml(</select></div><div class="S"><h2>Units</h2><label>Temp Unit</label><select name="faren">)rawhtml";
    html += "<option value=\"1\""; if (_useFahrenheit) html += " selected"; html += ">Fahrenheit (°F)</option>";
    html += "<option value=\"0\""; if (!_useFahrenheit) html += " selected"; html += ">Celsius (°C)</option>";
    html += R"rawhtml(</select></div><button type="submit">Save & Reboot</button></form></div><div class="C"><div><h2>Status</h2><div>)rawhtml";

    html += "<span class=\"badge\">WiFi: ";
    html += (isConnected() ? "OK" : "Offline");
    html += "</span><br><span class=\"badge\">IP: ";
    html += _localIP;
    html += "</span><br>";
    int t = (int)(_tempC + 0.5f);
    if (_useFahrenheit && _tempC > -90) t = (int)((_tempC * 9/5) + 32.5f);
    String weatherStr = (_tempC < -90) ? "--" : (String(t) + "°" + (_useFahrenheit ? "F" : "C") + " " + _weatherDesc);
    html += "<span class=\"badge\">";
    html += weatherStr;
    html += "</span>";

    html += R"rawhtml(</div></div><div class="S"><h2>Simulation</h2><form method="POST" action="/api/simulate" class="SR">
<button name="event" value="single" class="sb">Single</button>
<button name="event" value="double" class="sb">Double</button>
<button name="event" value="long" class="sb">Long</button>
</form><p class="ht">Single=next &bull; Double=info &bull; Long=love</p></div><div class="S"><h2>Buzzer</h2><form method="POST" action="/api/buzz"><div class="SR">
<button name="pattern" value="boot" class="sb">Boot</button>
<button name="pattern" value="tap" class="sb">Tap</button>
<button name="pattern" value="double" class="sb">Double</button>
<button name="pattern" value="long" class="sb">Long</button>
</div><div class="SR" style="margin-top:.35rem">
<button name="pattern" value="happy" class="sb">Happy</button>
<button name="pattern" value="sad" class="sb">Sad</button>
<button name="pattern" value="alert" class="sb">Alert</button>
<button name="pattern" value="starwars" class="sb">&#9733; Star Wars</button>
</div></form></div><div class="S"><h2>Expression</h2><form method="POST" action="/api/expression" class="FG">)rawhtml";

    static const char* expr_names[] = {"Happy","Neutral","Sad","Surprised","Love","Sleepy","Angry","Dead","Blink","Wink L","Wink R"};
    for (int i = 0; i < 11; i++) {
        html += "<button name=\"expr\" value=\"";
        html += String(i);
        html += "\" class=\"fb\">";
        html += expr_names[i];
        html += "</button>";
    }

    html += R"rawhtml(</form></div></div></div></div></body></html>)rawhtml";
    _server.sendHeader("Cache-Control", "no-cache");
    _server.send(200, "text/html", html);
}

void NetworkManager::handleSave() {
    String ssid = _server.arg("ssid");
    String pass = _server.arg("pass");
    String lat  = _server.arg("lat"); lat.trim();
    String lon  = _server.arg("lon"); lon.trim();
    String tz   = _server.arg("tz");
    String faren= _server.arg("faren");
    if (ssid.length() == 0) { _server.send(400, "text/plain", "SSID required"); return; }
    _prefs.putString("ssid", ssid);
    if (pass.length() > 0) _prefs.putString("pass", pass);
    _prefs.putString("lat",  lat.length() ? lat : "48.8566");
    _prefs.putString("lon",  lon.length() ? lon : "2.3522");
    _prefs.putLong("tz", tz.length() ? tz.toInt() : 0);
    _prefs.putBool("faren", faren == "1");
    _server.send(200, "text/html", "<html><body style='font-family:system-ui;background:#111;color:#eee;display:flex;justify-content:center;align-items:center;height:100vh'><div style='text-align:center'><h2 style='color:#6af'>✅ Saved!</h2><p style='margin-top:1rem'>YETI is rebooting…</p></div></body></html>");
    delay(1500); ESP.restart();
}

void NetworkManager::handleApiStatus() {
    JsonDocument doc;
    doc["ssid"]     = _ssid;
    doc["lat"]      = _lat;
    doc["lon"]      = _lon;
    doc["tz_sec"]   = _tzOffsetSec;
    doc["faren"]    = _useFahrenheit;
    doc["wifi_ok"]  = isConnected();
    doc["ip"]       = _localIP;
    doc["rssi"]     = isConnected() ? WiFi.RSSI() : 0;
    doc["temp_c"]   = _tempC;
    doc["weather"]  = _weatherDesc;
    doc["time"]     = _timeStr;
    String out; serializeJson(doc, out);
    _server.sendHeader("Cache-Control", "no-cache");
    _server.send(200, "application/json", out);
}

void NetworkManager::handleSimulate() {
    String evt = _server.arg("event");
    if      (evt == "single") _simulatedEvent = TOUCH_SINGLE;
    else if (evt == "double") _simulatedEvent = TOUCH_DOUBLE;
    else if (evt == "long")   _simulatedEvent = TOUCH_LONG;
    _server.sendHeader("Location", "/");
    _server.send(302, "text/plain", "");
}

void NetworkManager::handleApiExpression() {
    if (_server.hasArg("expr")) {
        int expr = _server.arg("expr").toInt();
        if (expr >= 0 && expr < EXPR_COUNT && expr != EXPR_BLINK) {
            _pendingExpression = (int8_t)expr;
        }
    }
    _server.sendHeader("Location", "/");
    _server.send(302, "text/plain", "");
}

void NetworkManager::handleApiBuzz() {
    String p = _server.arg("pattern");
    if      (p == "boot")   _pendingBuzzPattern = BUZZ_BOOT;
    else if (p == "tap")    _pendingBuzzPattern = BUZZ_TAP;
    else if (p == "double") _pendingBuzzPattern = BUZZ_DOUBLE_TAP;
    else if (p == "long")   _pendingBuzzPattern = BUZZ_LONG_PRESS;
    else if (p == "happy")  _pendingBuzzPattern = BUZZ_HAPPY;
    else if (p == "sad")    _pendingBuzzPattern = BUZZ_SAD;
    else if (p == "alert")    _pendingBuzzPattern = BUZZ_ALERT;
    else if (p == "starwars") _pendingBuzzPattern = BUZZ_STARWARS;
    _server.sendHeader("Location", "/");
    _server.send(302, "text/plain", "");
}

TouchEvent NetworkManager::consumeSimulatedEvent() { TouchEvent e = _simulatedEvent; _simulatedEvent = TOUCH_NONE; return e; }
int8_t NetworkManager::consumePendingExpression() { int8_t e = _pendingExpression; _pendingExpression = -1; return e; }
BuzzPattern NetworkManager::consumePendingBuzzPattern() { BuzzPattern p = _pendingBuzzPattern; _pendingBuzzPattern = BUZZ_NONE; return p; }
void NetworkManager::handleNotFound() { _server.sendHeader("Location", "/"); _server.send(302, "text/plain", "Redirect"); }

void NetworkManager::fetchWeather() {
    if (!isConnected()) return;
    char url[256];
    snprintf(url, sizeof(url), "https://api.open-meteo.com/v1/forecast?latitude=%s&longitude=%s&current=temperature_2m,weather_code&forecast_days=1", _lat.c_str(), _lon.c_str());
    HTTPClient http;
    http.begin(url);
    http.addHeader("User-Agent", "YETI-Companion/1.0");
    http.setTimeout(5000);
    int code = http.GET();
    if (code == 200) {
        String payload = http.getString();
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, payload);
        if (!err) {
            _tempC = doc["current"]["temperature_2m"] | -99.0f;
            int wcode = doc["current"]["weather_code"] | 0;
            if      (wcode == 0)                  strncpy(_weatherDesc, "Clear",   sizeof(_weatherDesc));
            else if (wcode <= 3)                  strncpy(_weatherDesc, "Cloudy",  sizeof(_weatherDesc));
            else if (wcode <= 48)                 strncpy(_weatherDesc, "Foggy",   sizeof(_weatherDesc));
            else if (wcode <= 67)                 strncpy(_weatherDesc, "Rain",    sizeof(_weatherDesc));
            else if (wcode <= 77)                 strncpy(_weatherDesc, "Snow",    sizeof(_weatherDesc));
            else if (wcode <= 82)                 strncpy(_weatherDesc, "Showers", sizeof(_weatherDesc));
            else if (wcode <= 99)                 strncpy(_weatherDesc, "Storm",   sizeof(_weatherDesc));
            else                                  strncpy(_weatherDesc, "---",     sizeof(_weatherDesc));
        }
    }
    http.end();
}

void NetworkManager::updateTime() {
    uint32_t now32 = (uint32_t)time(NULL);
    struct tm t;
    time_t now = (time_t)now32;
    localtime_r(&now, &t);
    if (t.tm_year < 100) return;
    if (!_ntpSynced) {
        Serial.printf("[NET] Time synced: %04d-%02d-%02d %02d:%02d:%02d\n", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec);
        _ntpSynced = true;
    }
    strftime(_timeStr, sizeof(_timeStr), "%H:%M", &t);
    strftime(_dateStr, sizeof(_dateStr), "%a %d %b", &t);
}

bool NetworkManager::isConnected() const { return _wifiConnected && (WiFi.status() == WL_CONNECTED); }
int NetworkManager::getRSSI() const { return isConnected() ? WiFi.RSSI() : 0; }
