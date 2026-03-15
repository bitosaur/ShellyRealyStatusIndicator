Shelly Gate Status Indicator
============================

ESP32-powered visual status indicator for your house gate using a Shelly relay.  
A bright, glanceable LED (or RGB) shows whether the gate is **open** or **closed** — visible from the driveway or yard, with smart day/night brightness control to minimize light pollution at night.

Features
--------

*   **Primary: Event-driven updates** — Shelly pushes status changes instantly via webhook / HTTP callback (recommended — low latency, minimal network traffic)
*   **Fallback: Periodic polling** — used only on boot, for initial state, or recovery after missed events / Wi-Fi issues
*   Full PWM control of LED color and intensity
*   Separate **daytime** and **nighttime** brightness levels (time-based; LDR support can be added easily)
*   Supports single RGB LED (common cathode/anode) or separate red + green LEDs
*   Compatible with Shelly Gen1 (via HTTP Actions) and Gen2/Plus/Pro (via Webhooks)
*   Simple ESP32 HTTP server to receive webhook pushes
*   Wi-Fi and Shelly configuration stored in `secrets.h`
*   MIT licensed

Typical Use Case – House Gate in Trumbull, CT
---------------------------------------------

Shelly relay controls/triggers the gate opener:

*   Relay **ON** → Gate is **open** → Red LED lit
*   Relay **OFF** → Gate is **closed** → Green LED lit

The indicator is mounted in a weatherproof enclosure, visible from outside.

Hardware Required
-----------------

*   ESP32 development board (any with Wi-Fi and enough PWM-capable GPIOs)
*   LED options:
    *   Common-cathode RGB LED (best for color mixing and future expansion)
    *   OR separate high-brightness red + green 5 mm / 10 mm LEDs
*   220–330 Ω current-limiting resistors (one per channel)
*   Optional: LDR (photoresistor) + 10 kΩ resistor for true ambient-light dimming
*   5 V USB/weatherproof power supply
*   Waterproof enclosure for outdoor mounting

### Example wiring – Common-cathode RGB LED

    ESP32 GPIO18 ── 220 Ω ── Red leg
    ESP32 GPIO19 ── 220 Ω ── Green leg
    ESP32 GPIO21 ── 220 Ω ── Blue leg (optional – can be left unconnected)
    Common cathode ── GND
    

For separate red/green LEDs, connect each anode → resistor → GPIO, cathode → GND.

Software / Firmware
-------------------

Written for **Arduino IDE** with ESP32 board support.

### Important files

*   Main sketch (e.g. `ShellyGateIndicator.ino`)
*   `secrets.h` (create from the provided `example.secrets.h`)

### Configuration

1.  Copy `example.secrets.h` → `secrets.h` and fill in:
    
        #define WIFI_SSID     "YourWiFiSSID"
        #define WIFI_PASSWORD "YourWiFiPassword"
        
        #define SHELLY_IP     "192.168.1.123"     // or mDNS hostname
        #define RELAY_ID      0                   // usually 0 or 1
        
    
2.  In the main sketch adjust if needed:
    
        // PWM-capable GPIO pins
        #define PIN_RED    18
        #define PIN_GREEN  19
        #define PIN_BLUE   21     // optional
        
        // Brightness levels (0–255)
        #define BRIGHT_DAY_RED    220
        #define BRIGHT_DAY_GREEN  180
        #define BRIGHT_NIGHT_RED   60
        #define BRIGHT_NIGHT_GREEN 45
        
        // Simple time-based day/night (24-hour format)
        #define DAY_START_HOUR   7
        #define DAY_END_HOUR    22
        
        // Logic – change if your relay is inverted
        #define RELAY_ON_MEANS_OPEN true
        
    

Recommended: Webhook / Push Mode (Primary)
------------------------------------------

Configure the Shelly to **push** state changes to the ESP32 instead of constant polling.

### Shelly Configuration

**Gen2 / Plus / Pro devices** (preferred – full webhook support):

1.  Open Shelly web interface → **Settings → Webhooks**
2.  Create webhook(s):
    *   Component: Switch:<RELAY\_ID> (e.g. Switch:0)
    *   Events: `switch.on` and/or `switch.off`
    *   URL: `http://<ESP32-IP>/webhook` (use static IP or mDNS)
    *   Enable and save

**Gen1 devices** (older firmware – uses Actions):

1.  Web UI → **Settings → Actions**
2.  Set:
    *   `out_on_url` = `http://<ESP32-IP>/webhook?state=on`
    *   `out_off_url` = `http://<ESP32-IP>/webhook?state=off`
3.  Enable actions

### ESP32 Webhook Handler (add to sketch)

    #include <WebServer.h>
    WebServer server(80);
    
    bool gateIsOpen = false;
    
    void handleWebhook() {
      if (server.method() == HTTP_POST) {
        String body = server.arg("plain");
    
        // Basic string check (for production use ArduinoJson)
        if (body.indexOf("\"output\":true") != -1 ||
            body.indexOf("\"event\":\"switch.on\"") != -1 ||
            server.arg("state") == "on") {
          gateIsOpen = true;
        } else if (body.indexOf("\"output\":false") != -1 ||
                   body.indexOf("\"event\":\"switch.off\"") != -1 ||
                   server.arg("state") == "off") {
          gateIsOpen = false;
        }
    
        updateLEDs();           // your function to set PWM based on time/brightness
        server.send(200, "text/plain", "OK");
      } else {
        server.send(400);
      }
    }
    
    void setup() {
      // WiFi connection code...
      server.on("/webhook", HTTP_POST, handleWebhook);
      server.begin();
    
      // Poll once on boot for initial state
      pollShellyStatus();
    }
    

In `loop()` add:

    server.handleClient();
    // Optional: if (millis() - lastWebhookTime > 600000) pollShellyStatus(); // 10 min fallback
    

Fallback Polling
----------------

Implement a simple poll function (runs on startup + optionally every 10–15 minutes):

*   Gen2: `GET http://SHELLY_IP/rpc/Switch.Get?id=RELAY_ID` → parse `"output"`
*   Gen1: `GET http://SHELLY_IP/relay/RELAY_ID` → parse `"ison"`

Optional Improvements
---------------------

*   Use **ArduinoJson** library for reliable payload parsing
*   Add webhook timeout → force poll if no events for >5–10 minutes
*   LDR-based dimming instead of clock time
*   Fast blue blink on Wi-Fi/Shelly connection loss
*   OTA firmware updates
*   Support for addressable LEDs (WS2812B) with patterns

License
-------

MIT License

Built for my own gate — feel free to adapt, improve, or submit pull requests!
