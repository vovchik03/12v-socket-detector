#pragma once
// Hardware configuration and tunables. Secrets (Wi-Fi, MQTT credentials) live in secrets.h.

#include <Arduino.h>

#define FW_VERSION "0.1.0"

// ---------------------------------------------------------------------------
// Inputs (voltage sense channels)
// ---------------------------------------------------------------------------
// Each 12V input is measured through a resistor divider into an ADC1 pin
// (ADC2 cannot be used while Wi-Fi is on). Add a new row to INPUT_CHANNELS to
// add an input — no logic changes required.

enum class InputRole : uint8_t {
  Main,    // mains adapter; its presence means "grid power is on"
  Backup,  // power bank / battery; voltage is tracked for low-charge warnings
  Generic  // just reported
};

struct InputChannelConfig {
  const char* name;      // used in MQTT payloads
  uint8_t pin;           // ADC1 pin: 32..39
  InputRole role;
  float dividerRatio;    // Vin / Vadc, e.g. (100k + 22k) / 22k
  float calibration;     // multiply measured volts by this (tune per board)
  float presentOnV;      // rising threshold: above -> present
  float presentOffV;     // falling threshold: below -> absent (hysteresis)
};

constexpr float DIVIDER_100K_22K = (100.0f + 22.0f) / 22.0f;  // ~5.545, 12.6V -> 2.27V

constexpr InputChannelConfig INPUT_CHANNELS[] = {
  // name      pin  role               divider           cal   on     off
  {"main",    34,  InputRole::Main,    DIVIDER_100K_22K, 1.0f, 10.5f, 9.0f},
  {"backup",  35,  InputRole::Backup,  DIVIDER_100K_22K, 1.0f, 10.5f, 9.0f},
};
constexpr size_t INPUT_CHANNEL_COUNT = sizeof(INPUT_CHANNELS) / sizeof(INPUT_CHANNELS[0]);

constexpr uint32_t INPUT_SAMPLE_INTERVAL_MS = 50;    // ADC sampling period
constexpr float    INPUT_FILTER_ALPHA       = 0.3f;  // EMA smoothing (0..1, higher = faster)
constexpr uint32_t INPUT_DEBOUNCE_MS        = 300;   // state must hold this long to count

// Backup low-charge warning (with hysteresis)
constexpr float BACKUP_LOW_V   = 11.2f;
constexpr float BACKUP_OK_V    = 11.8f;

// ---------------------------------------------------------------------------
// Output (12V / 1A load switch — the Wi-Fi router hangs on it)
// ---------------------------------------------------------------------------
constexpr uint8_t  OUTPUT_PIN          = 25;
constexpr bool     OUTPUT_ACTIVE_HIGH  = true;   // GPIO level that turns the load ON
constexpr bool     OUTPUT_DEFAULT_ON   = true;   // state after boot
// The router powers our own uplink: an "off" command is automatically reverted
// after this time so the device can never permanently cut its own connection.
constexpr bool     OUTPUT_FAILSAFE_ENABLED   = true;
constexpr uint32_t OUTPUT_FAILSAFE_DEFAULT_S = 300;   // "off" without duration
constexpr uint32_t OUTPUT_FAILSAFE_MAX_S     = 3600;  // cap for "off:<seconds>"

// ---------------------------------------------------------------------------
// Status LED (onboard LED on most ESP32 DevKit boards)
// ---------------------------------------------------------------------------
constexpr int8_t STATUS_LED_PIN = 2;  // -1 to disable

// ---------------------------------------------------------------------------
// Connectivity / telemetry
// ---------------------------------------------------------------------------
constexpr uint32_t HEARTBEAT_INTERVAL_MAIN_MS   = 30000;  // on grid power
constexpr uint32_t HEARTBEAT_INTERVAL_BACKUP_MS = 60000;  // on power bank (economy)
// The server watchdog timeout must be larger than the longest interval above.

constexpr uint16_t MQTT_KEEPALIVE_S        = 30;  // LWT fires ~1.5x this after silence
constexpr uint32_t MQTT_RECONNECT_MIN_MS   = 2000;
constexpr uint32_t MQTT_RECONNECT_MAX_MS   = 60000;
constexpr uint32_t WIFI_RECONNECT_MS       = 10000;
constexpr uint32_t STATE_PUBLISH_MIN_MS    = 1000;   // rate limit for retained state

constexpr const char* MQTT_TOPIC_PREFIX = "psd";  // psd/<device_id>/...

constexpr size_t EVENT_QUEUE_SIZE = 16;  // events buffered while offline

constexpr uint32_t TASK_WDT_TIMEOUT_S = 30;
