#ifndef SECRETS_H
#define SECRETS_H

const char* ssid          = "your_ssid";
const char* password      = "your_wifi_password";

// From Shelly App → Settings → Authorization cloud key
const char* AUTH_KEY      = "Your API Auth Key";

// Your Shelly cloud server (look in app or decode from token / network capture)
// Examples: shelly-15-eu.shelly.cloud, shelly-49-eu.shelly.cloud, shelly-27-us.shelly.cloud, etc.
const char* SHELLY_SERVER = "shelly-15-eu.shelly.cloud";

// Your Shelly 1 device ID (14 hexadecimal characters)
// Find it in Shelly Smart Control app → device settings → or via cloud API /device/all
const char* DEVICE_ID     = "987464321a123";

#endif
