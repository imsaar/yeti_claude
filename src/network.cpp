#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <time.h>
#include "network.h"

// ─── Embedded config web page ─────────────────────────────────────────────────
// Stored in program memory to save RAM
static const char CONFIG_HTML[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>YETI</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:system-ui,sans-serif;background:#111;color:#eee;display:flex;justify-content:center;align-items:flex-start;min-height:100vh;padding:1.5rem 1rem}
.W{background:#1e1e1e;border-radius:16px;padding:2rem;width:100%;max-width:820px;box-shadow:0 8px 32px #0008}
h1{font-size:1.6rem;font-weight:700;text-align:center;margin-bottom:1.5rem}
h1 span{color:#6af}
label{display:block;font-size:.8rem;color:#999;margin:1rem 0 .25rem}
input,select{width:100%;padding:.55rem .75rem;background:#2a2a2a;border:1px solid #444;border-radius:8px;color:#eee;font-size:.95rem;outline:none}
input:focus,select:focus{border-color:#6af}
.R{display:flex;gap:.75rem}.R>div{flex:1}
.G{display:grid;grid-template-columns:1fr 1fr;gap:0;align-items:start}
.C{border-left:1px solid #333;padding-left:2rem}
@media(max-width:580px){.G{grid-template-columns:1fr}.C{border-left:none;padding-left:0;border-top:1px solid #333;padding-top:1.5rem;margin-top:1.5rem}}
button{margin-top:1.5rem;width:100%;padding:.7rem;background:#6af;border:none;border-radius:8px;color:#111;font-size:1rem;font-weight:700;cursor:pointer}
button:hover{background:#89c4ff}
.err{color:#f88}
.S{margin-top:1.5rem;padding-top:1.5rem;border-top:1px solid #333}
.S0{margin-top:0;padding-top:0;border-top:none}
.S h2,.S0 h2{font-size:1rem;color:#aaa;margin-bottom:.5rem}
.badge{display:inline-block;background:#2a2a2a;border:1px solid #444;border-radius:6px;padding:.25rem .6rem;font-size:.8rem;margin:.15rem 0}
.SR{display:flex;gap:.5rem;margin-top:.5rem}.SR>form{flex:1}
.sb{margin-top:0;width:100%;padding:.6rem .25rem;background:#2a2a2a;border:1px solid #555;color:#eee;font-size:.85rem;font-weight:600}
.sb:hover{background:#3a3a3a}
.FG{display:grid;grid-template-columns:repeat(5,1fr);gap:.35rem;margin-top:.5rem}
.fb{margin-top:0;width:100%;padding:.45rem .1rem;font-size:.7rem;background:#2a2a2a;border:1px solid #555;color:#eee;font-weight:600;border-radius:8px;cursor:pointer}
.fb:hover{background:#3a3a3a}
.ht{font-size:.75rem;color:#666;margin-top:.35rem}
</style>
</head>
<body>
<div class="W">
<h1>&#x1F9CA; <span>YETI</span> Config</h1>
<div class="G">
<div style="padding-right:2rem">
<form id="cfg" action="/save" method="POST">
<div class="S0"><h2>WiFi</h2>
<label>SSID</label><input name="ssid" id="ssid" type="text" placeholder="MyNetwork" required>
<label>Password</label><input name="pass" id="pass" type="password" placeholder="(open if empty)">
</div>
<div class="S"><h2>Location</h2>
<div class="R"><div><label>Latitude</label><input name="lat" id="lat" type="text" placeholder="48.8566" required></div>
<div><label>Longitude</label><input name="lon" id="lon" type="text" placeholder="2.3522" required></div></div>
</div>
<div class="S"><h2>Timezone</h2>
<select name="tz" id="tz">
<option value="-43200">UTC-12</option><option value="-39600">UTC-11</option>
<option value="-36000">UTC-10</option><option value="-32400">UTC-9</option>
<option value="-28800">UTC-8</option><option value="-25200">UTC-7</option>
<option value="-21600">UTC-6</option><option value="-18000">UTC-5</option>
<option value="-14400">UTC-4</option><option value="-10800">UTC-3</option>
<option value="-7200">UTC-2</option><option value="-3600">UTC-1</option>
<option value="0" selected>UTC+0</option><option value="3600">UTC+1</option>
<option value="7200">UTC+2</option><option value="10800">UTC+3</option>
<option value="12600">UTC+3:30</option><option value="14400">UTC+4</option>
<option value="16200">UTC+4:30</option><option value="18000">UTC+5</option>
<option value="19800">UTC+5:30 (IST)</option><option value="20700">UTC+5:45</option>
<option value="21600">UTC+6</option><option value="23400">UTC+6:30</option>
<option value="25200">UTC+7</option><option value="28800">UTC+8</option>
<option value="32400">UTC+9 (JST)</option><option value="34200">UTC+9:30</option>
<option value="36000">UTC+10</option><option value="39600">UTC+11</option>
<option value="43200">UTC+12</option>
</select>
</div>
<button type="submit">Save &amp; Reboot</button>
</form>
</div>
<div class="C">
<div class="S0"><h2>Status</h2><div id="st">Loading&#x2026;</div></div>
<div class="S"><h2>Touch Simulation</h2>
<div class="SR">
<form method="POST" action="/api/simulate"><input type="hidden" name="event" value="single"><button type="submit" class="sb">Single Tap</button></form>
<form method="POST" action="/api/simulate"><input type="hidden" name="event" value="double"><button type="submit" class="sb">Double Tap</button></form>
<form method="POST" action="/api/simulate"><input type="hidden" name="event" value="long"><button type="submit" class="sb">Long Press</button></form>
</div>
<p class="ht">Single=next expr &#x2022; Double=info &#x2022; Long=love</p>
</div>
<div class="S"><h2>Expression</h2>
<div class="FG">
<form method="POST" action="/api/expression"><input type="hidden" name="expr" value="0"><button type="submit" class="fb">Happy</button></form>
<form method="POST" action="/api/expression"><input type="hidden" name="expr" value="1"><button type="submit" class="fb">Neutral</button></form>
<form method="POST" action="/api/expression"><input type="hidden" name="expr" value="2"><button type="submit" class="fb">Sad</button></form>
<form method="POST" action="/api/expression"><input type="hidden" name="expr" value="3"><button type="submit" class="fb">Surprised</button></form>
<form method="POST" action="/api/expression"><input type="hidden" name="expr" value="4"><button type="submit" class="fb">Love</button></form>
<form method="POST" action="/api/expression"><input type="hidden" name="expr" value="5"><button type="submit" class="fb">Sleepy</button></form>
<form method="POST" action="/api/expression"><input type="hidden" name="expr" value="6"><button type="submit" class="fb">Angry</button></form>
<form method="POST" action="/api/expression"><input type="hidden" name="expr" value="7"><button type="submit" class="fb">Dead</button></form>
<form method="POST" action="/api/expression"><input type="hidden" name="expr" value="9"><button type="submit" class="fb">Wink L</button></form>
<form method="POST" action="/api/expression"><input type="hidden" name="expr" value="10"><button type="submit" class="fb">Wink R</button></form>
</div>
</div>
</div>
</div>
</div>
<script>
function refreshStatus() {
  const el = document.getElementById('st');
  if (!el) return;
  console.log('Fetching status...');
  fetch('/api/status', { cache: 'no-store' }).then(r => {
    if (!r.ok) throw new Error('HTTP ' + r.status);
    return r.json();
  }).then(d => {
    console.log('Status received:', d);
    if (d.ssid) document.getElementById('ssid').value = d.ssid;
    if (d.lat) document.getElementById('lat').value = d.lat;
    if (d.lon) document.getElementById('lon').value = d.lon;
    if (d.tz_sec !== undefined) {
      const s = document.getElementById('tz');
      for (let i = 0; i < s.options.length; i++) {
        if (parseInt(s.options[i].value) === d.tz_sec) {
          s.selectedIndex = i;
          break;
        }
      }
    }
    const tc = (d.temp_c !== undefined && d.temp_c > -90) ? Math.round(d.temp_c) : '--';
    el.innerHTML = 
      '<span class="badge">WiFi: ' + (d.wifi_ok ? '&#x2705; Connected' : '&#x274C; Offline') + '</span><br>' +
      '<span class="badge">IP: ' + (d.ip || '--') + '</span><br>' +
      '<span class="badge">' + tc + '&#xB0;C ' + (d.weather || '--') + '</span>';
  }).catch(e => {
    console.error('Status error:', e);
    el.innerHTML = '<span class="err">Status unavailable (retrying...)</span>';
    setTimeout(refreshStatus, 3000);
  });
}
window.addEventListener('load', refreshStatus);
document.getElementById('cfg').addEventListener('submit', function(e) {
  const b = e.target.querySelector('button[type=submit]');
  b.textContent = 'Saving\u2026';
  b.disabled = true;
});
</script>
</body>
</html>
)rawhtml";

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

    if (_ssid.length() == 0) {
        // No credentials stored → AP mode
        startAPMode();
    } else {
        startSTA();
    }
    setupWebServer();
}

// ─── update() ────────────────────────────────────────────────────────────────
void NetworkManager::update() {
    _server.handleClient();

    uint32_t now = millis();

    if (!_apMode && _wifiConnected) {
        // Sync time every second
        if (now - _lastTimeMs >= 1000) {
            _lastTimeMs = now;
            updateTime();
        }
        // Refresh weather
        uint32_t interval = (_tempC < -90) ? 60000UL : WEATHER_INTERVAL_MS; // 1 min retry if no data
        if (_lastWeatherMs == 0 || now - _lastWeatherMs >= interval) {
            _lastWeatherMs = now;
            fetchWeather();
        }
    }
}

// ─── startAPMode() ───────────────────────────────────────────────────────────
void NetworkManager::startAPMode() {
    _apMode = true;
    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_SSID, AP_PASS);
    snprintf(_localIP, sizeof(_localIP), "192.168.4.1");
    Serial.printf("[NET] AP mode: SSID=%s IP=%s\n", AP_SSID, _localIP);
}

// ─── STA connect ─────────────────────────────────────────────────────────────
void NetworkManager::startSTA() {
    _apMode = false;
    WiFi.mode(WIFI_STA);
    WiFi.setHostname(HOSTNAME);
    WiFi.begin(_ssid.c_str(), _pass.c_str());

    Serial.printf("[NET] Connecting to %s …\n", _ssid.c_str());

    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
        delay(300);
        Serial.print('.');
    }

    if (WiFi.status() == WL_CONNECTED) {
        _wifiConnected = true;
        strncpy(_localIP, WiFi.localIP().toString().c_str(), sizeof(_localIP) - 1);
        Serial.printf("\n[NET] Connected! IP: %s\n", _localIP);
        setupMDNS();
        configTime(_tzOffsetSec, 0, NTP_SERVER);
    } else {
        Serial.println("\n[NET] Failed to connect → offline mode");
        _wifiConnected = false;
    }
}

void NetworkManager::setupMDNS() {
    if (MDNS.begin(HOSTNAME)) {
        MDNS.addService("http", "tcp", 80);
        Serial.printf("[NET] mDNS: http://%s.local\n", HOSTNAME);
    }
}

// ─── Web server ───────────────────────────────────────────────────────────────
void NetworkManager::setupWebServer() {
    _server.on("/",              [this]() { handleRoot(); });      // HTTP_ANY — captive portal compat
    _server.on("/save",          HTTP_POST, [this]() { handleSave(); });
    _server.on("/api/status",    [this]() { handleApiStatus(); }); // HTTP_ANY
    _server.on("/api/simulate",  HTTP_POST, [this]() { handleSimulate(); });
    _server.on("/api/expression",HTTP_POST, [this]() { handleApiExpression(); });
    // Silence the common browser auto-request so it doesn't pollute the log
    _server.on("/favicon.ico",   HTTP_GET,  [this]() { _server.send(204, "text/plain", ""); });
    _server.onNotFound([this]() { handleNotFound(); });
    _server.begin();
}

void NetworkManager::handleRoot() {
    Serial.printf("[NET] Serving config page to %s\n", _server.client().remoteIP().toString().c_str());
    _server.sendHeader("Cache-Control", "no-cache");
    _server.send_P(200, "text/html", CONFIG_HTML);
}

void NetworkManager::handleSave() {
    // ... (rest of handleSave remains the same)
    String ssid = _server.arg("ssid");
    String pass = _server.arg("pass");
    String lat  = _server.arg("lat"); lat.trim();
    String lon  = _server.arg("lon"); lon.trim();
    String tz   = _server.arg("tz");

    if (ssid.length() == 0) {
        _server.send(400, "text/plain", "SSID required");
        return;
    }

    _prefs.putString("ssid", ssid);
    _prefs.putString("pass", pass);
    _prefs.putString("lat",  lat.length() ? lat : "48.8566");
    _prefs.putString("lon",  lon.length() ? lon : "2.3522");
    _prefs.putLong("tz", tz.length() ? tz.toInt() : 0);

    // Serve confirmation then reboot
    _server.send(200, "text/html",
        "<html><body style='font-family:system-ui;background:#111;color:#eee;"
        "display:flex;justify-content:center;align-items:center;height:100vh'>"
        "<div style='text-align:center'>"
        "<h2 style='color:#6af'>&#x2705; Saved!</h2>"
        "<p style='margin-top:1rem'>YETI is rebooting…</p>"
        "</div></body></html>");
    delay(1500);
    ESP.restart();
}

void NetworkManager::handleApiStatus() {
    uint32_t start = millis();
    Serial.println("[NET] API Status: Received request");
    
    JsonDocument doc;
    doc["ssid"]     = _ssid;
    doc["lat"]      = _lat;
    doc["lon"]      = _lon;
    doc["tz_sec"]   = _tzOffsetSec;
    doc["wifi_ok"]  = _wifiConnected;
    doc["ap_mode"]  = _apMode;
    doc["ip"]       = _localIP;
    doc["rssi"]     = _wifiConnected ? WiFi.RSSI() : 0;
    doc["temp_c"]   = _tempC;
    doc["weather"]  = _weatherDesc;
    doc["time"]     = _timeStr;

    String out;
    if (serializeJson(doc, out) == 0) {
        Serial.println("[NET] API Status: Serialization failed");
        _server.send(500, "application/json", "{\"error\":\"serialize failed\"}");
        return;
    }
    
    _server.sendHeader("Cache-Control", "no-cache");
    _server.sendHeader("Connection", "close");
    _server.send(200, "application/json", out);
    Serial.printf("[NET] API Status: Sent in %ums. JSON: %s\n", (uint32_t)(millis() - start), out.c_str());
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

TouchEvent NetworkManager::consumeSimulatedEvent() {
    TouchEvent e = _simulatedEvent;
    _simulatedEvent = TOUCH_NONE;
    return e;
}

int8_t NetworkManager::consumePendingExpression() {
    int8_t e = _pendingExpression;
    _pendingExpression = -1;
    return e;
}

void NetworkManager::handleNotFound() {
    _server.sendHeader("Location", "/");
    _server.send(302, "text/plain", "Redirect");
}

// ─── Weather (Open-Meteo, no API key needed) ──────────────────────────────────
void NetworkManager::fetchWeather() {
    if (!_wifiConnected) {
        Serial.println("[WEATHER] WiFi not connected, skipping fetch.");
        return;
    }
    
    char url[256];
    snprintf(url, sizeof(url),
        "https://api.open-meteo.com/v1/forecast"
        "?latitude=%s&longitude=%s"
        "&current=temperature_2m,weather_code"
        "&forecast_days=1",
        _lat.c_str(), _lon.c_str());

    Serial.printf("[WEATHER] Fetching from: %s\n", url);

    HTTPClient http;
    // For ESP32 HTTPClient, https requires either a root CA or setInsecure()
    http.begin(url);
    http.addHeader("User-Agent", "YETI-Companion/1.0 (esp32-c3; contact:yeti@local)");
    http.setTimeout(5000); // 5s timeout
    int code = http.GET();

    if (code == 200) {
        String payload = http.getString();
        Serial.printf("[WEATHER] HTTP 200, Payload: %s\n", payload.c_str());
        
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, payload);
        if (!err) {
            _tempC = doc["current"]["temperature_2m"] | -99.0f;
            int wcode = doc["current"]["weather_code"] | 0;

            // Map WMO code → short description
            if      (wcode == 0)                  strncpy(_weatherDesc, "Clear",   sizeof(_weatherDesc));
            else if (wcode <= 3)                  strncpy(_weatherDesc, "Cloudy",  sizeof(_weatherDesc));
            else if (wcode <= 48)                 strncpy(_weatherDesc, "Foggy",   sizeof(_weatherDesc));
            else if (wcode <= 67)                 strncpy(_weatherDesc, "Rain",    sizeof(_weatherDesc));
            else if (wcode <= 77)                 strncpy(_weatherDesc, "Snow",    sizeof(_weatherDesc));
            else if (wcode <= 82)                 strncpy(_weatherDesc, "Showers", sizeof(_weatherDesc));
            else if (wcode <= 99)                 strncpy(_weatherDesc, "Storm",   sizeof(_weatherDesc));
            else                                  strncpy(_weatherDesc, "---",     sizeof(_weatherDesc));

            Serial.printf("[WEATHER] Success: %d°C, %s (code %d)\n", (int)(_tempC + 0.5f), _weatherDesc, wcode);
        } else {
            Serial.printf("[WEATHER] JSON parse error: %s\n", err.c_str());
        }
    } else {
        Serial.printf("[WEATHER] HTTP error: %d\n", code);
    }
    http.end();
}

// ─── NTP time ─────────────────────────────────────────────────────────────────
void NetworkManager::updateTime() {
    struct tm t;
    if (!getLocalTime(&t, 0)) return;   // 0 ms wait — non-blocking

    strftime(_timeStr, sizeof(_timeStr), "%H:%M", &t);
    strftime(_dateStr, sizeof(_dateStr), "%a %d %b", &t);
}

// ─── Getters ──────────────────────────────────────────────────────────────────
bool NetworkManager::isConnected() const {
    return _wifiConnected && (WiFi.status() == WL_CONNECTED);
}

int NetworkManager::getRSSI() const {
    return _wifiConnected ? WiFi.RSSI() : 0;
}
