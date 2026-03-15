// Display status of Shelly 1 relay in ESP32S3-DevKit-C1 onboard RGB LED
// Red    = Relay OFF
// Green  = Relay ON
// Yellow = Unknown / no recent update / error

#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <Adafruit_NeoPixel.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <time.h>

// ────────────────────────────────────────────────
// DEFAULTS

uint32_t COLOR_ON_DEFAULT      = 0x00FF00;
uint32_t COLOR_OFF_DEFAULT     = 0xFF0000;
uint32_t COLOR_UNKNOWN_DEFAULT = 0xFFFF00;
const uint8_t BRIGHTNESS_DEFAULT       = 50;
const uint8_t NIGHT_BRIGHTNESS_DEFAULT = 25;
const char*   SHELLY_IP_DEFAULT        = "192.168.1.102";
// Force mode: 0 = Auto, 1 = Force Day, 2 = Force Night
const uint8_t FORCE_MODE_DEFAULT = 0;
// ────────────────────────────────────────────────

Adafruit_NeoPixel strip(1, 48, NEO_GRB + NEO_KHZ800);

AsyncWebServer server(80);
Preferences prefs;

uint32_t colorOn      = COLOR_ON_DEFAULT;
uint32_t colorOff     = COLOR_OFF_DEFAULT;
uint32_t colorUnknown = COLOR_UNKNOWN_DEFAULT;
uint8_t  brightness   = BRIGHTNESS_DEFAULT;
uint8_t  nightBrightness = NIGHT_BRIGHTNESS_DEFAULT;
String   shellyIp     = SHELLY_IP_DEFAULT;
uint8_t  forceMode    = FORCE_MODE_DEFAULT;  // 0=auto, 1=day, 2=night

int lastKnownState = -1;
unsigned long lastStateUpdateMillis = 0;
unsigned long lastPollMillis = 0;

const uint16_t poll_interval_minutes = 30;
const unsigned long STATE_TIMEOUT_SEC = 3600;

#include "secrets.h"  // ssid, password

// US Eastern Time with automatic DST
void initTime() {
  setenv("TZ", "EST5EDT,M3.2.0/2,M11.1.0/2", 1);  // America/New_York equivalent
  tzset();

  configTime(0, 0, "pool.ntp.org");
  Serial.print("Waiting for NTP...");
  time_t now = time(nullptr);
  int tries = 0;
  while (now < 1000000000 && tries < 30) {
    delay(500);
    Serial.print(".");
    now = time(nullptr);
    tries++;
  }
  Serial.println(now > 1000000000 ? "\nTime synced" : "\nNTP failed - using no-time fallback");
}

bool isNightTime() {
  if (forceMode == 1) return false;  // Force Day
  if (forceMode == 2) return true;   // Force Night

  // Auto mode - check real time
  time_t now;
  struct tm timeinfo;
  time(&now);
  if (!localtime_r(&now, &timeinfo)) {
    return false;  // fallback to day if time invalid
  }

  int hour = timeinfo.tm_hour;
  return (hour >= 22 || hour < 6);
}

uint8_t getCurrentBrightness() {
  return isNightTime() ? nightBrightness : brightness;
}

void setLed(uint32_t color) {
  strip.setPixelColor(0, color);
  strip.setBrightness(getCurrentBrightness());
  strip.show();
}

void updateLed() {
  unsigned long now = millis();
  bool fresh = (now - lastStateUpdateMillis < STATE_TIMEOUT_SEC * 1000UL);

  if (lastKnownState == 1 && fresh)      setLed(colorOn);
  else if (lastKnownState == 0 && fresh) setLed(colorOff);
  else                                   setLed(colorUnknown);
}

void loadSettings() {
  prefs.begin("shelly_led", false);

  colorOn        = prefs.getULong("color_on",      COLOR_ON_DEFAULT);
  colorOff       = prefs.getULong("color_off",     COLOR_OFF_DEFAULT);
  colorUnknown   = prefs.getULong("color_unknown", COLOR_UNKNOWN_DEFAULT);
  brightness     = prefs.getUChar("brightness",    BRIGHTNESS_DEFAULT);
  nightBrightness = prefs.getUChar("night_br",     NIGHT_BRIGHTNESS_DEFAULT);
  forceMode      = prefs.getUChar("force_mode",    FORCE_MODE_DEFAULT);

  String savedIp = prefs.getString("shelly_ip", SHELLY_IP_DEFAULT);
  shellyIp = savedIp.length() > 0 ? savedIp : SHELLY_IP_DEFAULT;

  prefs.end();

  Serial.printf("[Settings] Loaded: ON=0x%06X OFF=0x%06X UNK=0x%06X DayBr=%d NightBr=%d Force=%d IP=%s\n",
                colorOn, colorOff, colorUnknown, brightness, nightBrightness, forceMode, shellyIp.c_str());
}

void saveSettings(uint32_t cOn, uint32_t cOff, uint32_t cUnk, uint8_t br, uint8_t nbr, uint8_t fmode, String ip) {
  prefs.begin("shelly_led", false);
  prefs.putULong("color_on",      cOn);
  prefs.putULong("color_off",     cOff);
  prefs.putULong("color_unknown", cUnk);
  prefs.putUChar("brightness",    br);
  prefs.putUChar("night_br",      nbr);
  prefs.putUChar("force_mode",    fmode);
  prefs.putString("shelly_ip",    ip);
  prefs.end();

  colorOn        = cOn;
  colorOff       = cOff;
  colorUnknown   = cUnk;
  brightness     = br;
  nightBrightness = nbr;
  forceMode      = fmode;
  shellyIp       = ip;

  updateLed();
  Serial.println("[Settings] Saved");
}

// ────────────────────────────────────────────────
// URL Action handlers (unchanged)
// ────────────────────────────────────────────────

void handleButtonOn(AsyncWebServerRequest *request) {
  lastKnownState = 1; lastStateUpdateMillis = millis();
  Serial.println("[URL] Button ON"); updateLed();
  request->send(200, "text/plain", "OK");
}

void handleButtonOff(AsyncWebServerRequest *request) {
  lastKnownState = 0; lastStateUpdateMillis = millis();
  Serial.println("[URL] Button OFF"); updateLed();
  request->send(200, "text/plain", "OK");
}

void handleButtonLong(AsyncWebServerRequest *request) {
  Serial.println("[URL] Button long"); request->send(200, "text/plain", "OK");
}

void handleButtonShort(AsyncWebServerRequest *request) {
  Serial.println("[URL] Button short"); request->send(200, "text/plain", "OK");
}

void handleOutputOn(AsyncWebServerRequest *request) {
  lastKnownState = 1; lastStateUpdateMillis = millis();
  Serial.println("[URL] Output ON"); updateLed();
  request->send(200, "text/plain", "OK");
}

void handleOutputOff(AsyncWebServerRequest *request) {
  lastKnownState = 0; lastStateUpdateMillis = millis();
  Serial.println("[URL] Output OFF"); updateLed();
  request->send(200, "text/plain", "OK");
}

void pollShellyStatus() {
  if (WiFi.status() != WL_CONNECTED) return;

  HTTPClient http;
  String url = "http://" + shellyIp + "/status";
  http.begin(url);
  http.setTimeout(6000);

  int code = http.GET();
  if (code == HTTP_CODE_OK) {
    String payload = http.getString();
    if (payload.indexOf("\"relay0\":true") != -1 || payload.indexOf("\"ison\":true") != -1) {
      lastKnownState = 1;
    } else if (payload.indexOf("\"relay0\":false") != -1 || payload.indexOf("\"ison\":false") != -1) {
      lastKnownState = 0;
    } else {
      lastKnownState = -1;
    }
    lastStateUpdateMillis = millis();
    updateLed();
    Serial.println("[POLL] OK");
  } else {
    Serial.printf("[POLL] Fail %d\n", code);
  }
  http.end();
}

// ────────────────────────────────────────────────
// Config page
// ────────────────────────────────────────────────

void handleConfig(AsyncWebServerRequest *request) {
  String html = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>GATE Monitor Config</title>
  <style>
    body { font-family: Arial; max-width: 600px; margin: 20px auto; padding: 20px; background: #f8f8f8; }
    h1 { color: #333; }
    label { display: block; margin: 15px 0 5px; font-weight: bold; }
    input[type=color], input[type=number], input[type=text], input[type=radio] { margin: 5px; }
    .radio-group { margin: 10px 0; }
    button { margin-top: 20px; padding: 12px 20px; background: #4CAF50; color: white; border: none; cursor: pointer; }
    button:hover { background: #45a049; }
    .urls { margin-top: 40px; border-top: 1px solid #ccc; padding-top: 20px; }
    .url-item { margin: 10px 0; word-break: break-all; }
  </style>
</head>
<body>
  <h1>GATE Monitor Config</h1>

  <form action="/save" method="POST">
    <label>Relay ON color:</label>
    <input type="color" name="color_on" value="#%06X">

    <label>Relay OFF color:</label>
    <input type="color" name="color_off" value="#%06X">

    <label>Unknown/error color:</label>
    <input type="color" name="color_unknown" value="#%06X">

    <label>Daytime brightness (0–255):</label>
    <input type="number" name="brightness" min="0" max="255" value="%d">

    <label>Nighttime brightness (22:00–06:00, 0–255):</label>
    <input type="number" name="night_brightness" min="0" max="255" value="%d">

    <label>Brightness mode:</label>
    <div class="radio-group">
      <input type="radio" name="force_mode" value="0" %s> Auto (time-based)<br>
      <input type="radio" name="force_mode" value="1" %s> Force Day brightness<br>
      <input type="radio" name="force_mode" value="2" %s> Force Night brightness
    </div>

    <label>GATE Shelly 1 IP:</label>
    <input type="text" name="shelly_ip" value="%s" placeholder="192.168.1.102">

    <div class="savePrefs"> 
      <button type="submit">Save</button>
    </div>  
  </form>

  <div class="urls">
    <h2>API URLs for Shelly Actions</h2>
    <div class="url-item"><strong>Button ON:</strong> http://%s/button-on</div>
    <div class="url-item"><strong>Button OFF:</strong> http://%s/button-off</div>
    <div class="url-item"><strong>Button long:</strong> http://%s/button-long</div>
    <div class="url-item"><strong>Button short:</strong> http://%s/button-short</div>
    <div class="url-item"><strong>Output ON:</strong> http://%s/output-on</div>
    <div class="url-item"><strong>Output OFF:</strong> http://%s/output-off</div>
  </div>

  <p style="margin-top:30px; font-size:0.9em; color:#666;">
    ESP32 IP: %s<br>
    Current mode: %s<br>
    Applied brightness: %d<br>
    Last poll: %s ago
  </p>
</body>
</html>
)rawliteral";

  String ipStr = WiFi.localIP().toString();
  unsigned long sincePoll = (millis() - lastPollMillis) / 1000UL;
  String modeStr;
  if (forceMode == 1) modeStr = "Forced Day";
  else if (forceMode == 2) modeStr = "Forced Night";
  else modeStr = isNightTime() ? "Night (Auto)" : "Day (Auto)";

  char autoChecked[10] = "", dayChecked[10] = "", nightChecked[10] = "";
  if (forceMode == 0) strcpy(autoChecked, "checked");
  else if (forceMode == 1) strcpy(dayChecked, "checked");
  else strcpy(nightChecked, "checked");

  char buf[5000];
  snprintf(buf, sizeof(buf), html.c_str(),
    colorOn, colorOff, colorUnknown,
    brightness, nightBrightness,
    autoChecked, dayChecked, nightChecked,
    shellyIp.c_str(),
    ipStr.c_str(), ipStr.c_str(), ipStr.c_str(), ipStr.c_str(), ipStr.c_str(), ipStr.c_str(),
    ipStr.c_str(),
    modeStr.c_str(),
    getCurrentBrightness(),
    sincePoll < 120 ? String(sincePoll) + " sec" : String(sincePoll / 60) + " min"
  );

  request->send(200, "text/html", buf);
}

void handleSave(AsyncWebServerRequest *request) {
  if (!request->hasParam("color_on", true)) {
    request->send(400, "text/plain", "Bad request");
    return;
  }

  String cOnHex  = request->getParam("color_on", true)->value();
  String cOffHex = request->getParam("color_off", true)->value();
  String cUnkHex = request->getParam("color_unknown", true)->value();
  String brStr   = request->getParam("brightness", true)->value();
  String nbrStr  = request->getParam("night_brightness", true)->value();
  String fmStr   = request->getParam("force_mode", true)->value();
  String newIp   = request->getParam("shelly_ip", true)->value();

  uint32_t cOn  = strtol(cOnHex.startsWith("#") ? cOnHex.substring(1).c_str() : cOnHex.c_str(), NULL, 16);
  uint32_t cOff = strtol(cOffHex.startsWith("#") ? cOffHex.substring(1).c_str() : cOffHex.c_str(), NULL, 16);
  uint32_t cUnk = strtol(cUnkHex.startsWith("#") ? cUnkHex.substring(1).c_str() : cUnkHex.c_str(), NULL, 16);

  uint8_t br  = constrain(brStr.toInt(), 0, 255);
  uint8_t nbr = constrain(nbrStr.toInt(), 0, 255);
  uint8_t fm  = constrain(fmStr.toInt(), 0, 2);

  saveSettings(cOn, cOff, cUnk, br, nbr, fm, newIp.length() > 0 ? newIp : shellyIp);

  request->redirect("/");
}

void setup() {
  Serial.begin(115200);
  delay(300);

  strip.begin();
  strip.show();
  setLed(colorUnknown);

  loadSettings();

  initTime();

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi → " + WiFi.localIP().toString());

  server.on("/button-on",     HTTP_GET, handleButtonOn);
  server.on("/button-off",    HTTP_GET, handleButtonOff);
  server.on("/button-long",   HTTP_GET, handleButtonLong);
  server.on("/button-short",  HTTP_GET, handleButtonShort);
  server.on("/output-on",     HTTP_GET, handleOutputOn);
  server.on("/output-off",    HTTP_GET, handleOutputOff);

  server.on("/", HTTP_GET, handleConfig);
  server.on("/save", HTTP_POST, handleSave);

  server.begin();
  Serial.println("Server started – http://" + WiFi.localIP().toString());

  pollShellyStatus();
}

void loop() {
  unsigned long now = millis();

  if (now - lastPollMillis >= poll_interval_minutes * 60UL * 1000UL) {
    lastPollMillis = now;
    pollShellyStatus();
  }

  static unsigned long lastCheck = 0;
  if (now - lastCheck > 600000UL) {  // 10 min
    lastCheck = now;
    updateLed();  // re-apply brightness if mode changed
  }

  delay(20);
}
