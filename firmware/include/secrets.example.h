#pragma once
// Copy this file to secrets.h and fill in your values. secrets.h is git-ignored.

#define WIFI_SSID     "your-ssid"
#define WIFI_PASSWORD "your-password"

#define MQTT_HOST     "mqtt.example.com"
#define MQTT_PORT     8883          // 8883 = TLS, 1883 = plain (local testing only)
#define MQTT_USE_TLS  1             // 0 for plain TCP
#define MQTT_USER     "psd-device"
#define MQTT_PASSWORD "change-me"

// Leave empty to derive "psd-XXXXXX" from the MAC address.
#define DEVICE_ID     ""

// Root CA of the broker's certificate (PEM). For Let's Encrypt use ISRG Root X1:
// https://letsencrypt.org/certs/isrgrootx1.pem
// Empty string + MQTT_USE_TLS=1 => encrypted but the server is NOT verified (dev only).
static const char MQTT_CA_CERT[] = R"PEM(
)PEM";
