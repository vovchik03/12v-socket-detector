// 12V Socket Detector — ESP32 firmware entry point.
// See ../PLAN.md for the architecture and MQTT protocol.

#include <Arduino.h>
#include <ArduinoJson.h>
#include <esp_system.h>
#include <esp_task_wdt.h>

#include "config.h"
#include "event_queue.h"
#include "mqtt_link.h"
#include "output_control.h"
#include "power_monitor.h"

namespace {

PowerMonitor power;
OutputControl output;
MqttLink net;
EventQueue events;

const char* currentSource = "none";
bool backupLow = false;
bool stateDirty = true;
uint32_t lastStateMs = 0;
uint32_t lastHeartbeatMs = 0;
uint32_t heartbeatSeq = 0;
bool heartbeatNow = false;

float round2(float v) { return roundf(v * 100.0f) / 100.0f; }

const char* resetReasonStr(esp_reset_reason_t r) {
  switch (r) {
    case ESP_RST_POWERON:   return "poweron";
    case ESP_RST_EXT:       return "external";
    case ESP_RST_SW:        return "software";
    case ESP_RST_PANIC:     return "panic";
    case ESP_RST_INT_WDT:   return "int_wdt";
    case ESP_RST_TASK_WDT:  return "task_wdt";
    case ESP_RST_WDT:       return "wdt";
    case ESP_RST_DEEPSLEEP: return "deepsleep";
    case ESP_RST_BROWNOUT:  return "brownout";
    case ESP_RST_SDIO:      return "sdio";
    default:                return "unknown";
  }
}

void fillInputs(JsonObject obj) {
  for (size_t i = 0; i < power.count(); i++) {
    JsonObject ch = obj[power.config(i).name].to<JsonObject>();
    ch["v"] = round2(power.volts(i));
    ch["present"] = power.present(i);
    if (power.config(i).role == InputRole::Backup) ch["low"] = backupLow;
  }
}

// ---------------------------------------------------------------------------
// Event producers
// ---------------------------------------------------------------------------

void updateSource() {
  const char* src = power.source();
  if (strcmp(src, currentSource) == 0) return;
  JsonDocument& e = events.push("source");
  e["from"] = currentSource;
  e["to"] = src;
  log_i("power source: %s -> %s", currentSource, src);
  currentSource = src;
  stateDirty = true;
}

void onInputChange(size_t ch, bool present, float volts) {
  JsonDocument& e = events.push("input");
  e["input"] = power.config(ch).name;
  e["present"] = present;
  e["v"] = round2(volts);
  log_i("input %s: %s (%.2f V)", power.config(ch).name, present ? "present" : "absent", volts);
  stateDirty = true;
  updateSource();
}

void checkBackupLow() {
  const int b = power.indexOf(InputRole::Backup);
  if (b < 0 || !power.present(b)) return;
  const float v = power.volts(b);
  const bool low = backupLow ? (v < BACKUP_OK_V) : (v < BACKUP_LOW_V);
  if (low == backupLow) return;
  backupLow = low;
  JsonDocument& e = events.push(low ? "backup_low" : "backup_ok");
  e["input"] = power.config(b).name;
  e["v"] = round2(v);
  log_w("backup %s (%.2f V)", low ? "LOW" : "ok", v);
  stateDirty = true;
}

void onOutputChange(bool on, const char* reason) {
  JsonDocument& e = events.push("output");
  e["on"] = on;
  e["reason"] = reason;
  if (!on) e["failsafe_s"] = output.failsafeRemainingS();
  log_i("output %s (%s)", on ? "ON" : "OFF", reason);
  stateDirty = true;
}

// ---------------------------------------------------------------------------
// Commands: psd/<id>/cmd/<name>
// ---------------------------------------------------------------------------

void onCommand(const char* sub, const char* payload) {
  log_i("command %s = '%s'", sub, payload);

  if (strcmp(sub, "cmd/out") == 0) {
    if (strcmp(payload, "on") == 0) {
      output.turnOn("command");
    } else if (strcmp(payload, "off") == 0) {
      output.turnOff("command");
    } else if (strncmp(payload, "off:", 4) == 0) {
      output.turnOff("command", strtoul(payload + 4, nullptr, 10));
    } else if (strcmp(payload, "toggle") == 0) {
      output.isOn() ? output.turnOff("command") : output.turnOn("command");
    } else {
      log_w("unknown cmd/out payload");
    }
  } else if (strcmp(sub, "cmd/ping") == 0) {
    heartbeatNow = true;
    stateDirty = true;
  } else if (strcmp(sub, "cmd/reboot") == 0 && strcmp(payload, "yes") == 0) {
    net.publish("status", "offline", true);
    delay(200);
    ESP.restart();
  }
}

// ---------------------------------------------------------------------------
// Publishing
// ---------------------------------------------------------------------------

void onMqttConnected() {
  stateDirty = true;
  heartbeatNow = true;
}

void publishState() {
  JsonDocument doc;
  doc["fw"] = FW_VERSION;
  doc["source"] = currentSource;
  JsonObject out = doc["out"].to<JsonObject>();
  out["on"] = output.isOn();
  out["failsafe_s"] = output.failsafeRemainingS();
  fillInputs(doc["inputs"].to<JsonObject>());
  if (net.publish("state", doc, true)) {
    stateDirty = false;
    lastStateMs = millis();
  }
}

void publishHeartbeat() {
  JsonDocument doc;
  doc["seq"] = ++heartbeatSeq;
  doc["uptime_s"] = millis() / 1000;
  doc["rssi"] = net.rssi();
  doc["source"] = currentSource;
  doc["out"] = output.isOn();
  doc["heap"] = ESP.getFreeHeap();
  doc["dropped"] = events.dropped();
  JsonObject v = doc["v"].to<JsonObject>();
  for (size_t i = 0; i < power.count(); i++) v[power.config(i).name] = round2(power.volts(i));
  net.publish("heartbeat", doc);
}

void flushEvents() {
  while (!events.empty()) {
    EventQueue::Event& e = events.front();
    // Lets the server reconstruct when the event actually happened.
    e.doc["age_ms"] = millis() - e.createdMs;
    if (!net.publish("event", e.doc)) return;  // keep it for the next attempt
    events.pop();
  }
}

void serviceTelemetry() {
  if (!net.connected()) return;
  const uint32_t now = millis();

  flushEvents();

  if (stateDirty && now - lastStateMs >= STATE_PUBLISH_MIN_MS) publishState();

  const uint32_t interval = strcmp(currentSource, "backup") == 0 ? HEARTBEAT_INTERVAL_BACKUP_MS
                                                                 : HEARTBEAT_INTERVAL_MAIN_MS;
  if (heartbeatNow || now - lastHeartbeatMs >= interval) {
    heartbeatNow = false;
    lastHeartbeatMs = now;
    publishHeartbeat();
  }
}

// ---------------------------------------------------------------------------
// Status LED: solid = MQTT up, slow blink = Wi-Fi only, fast blink = no Wi-Fi
// ---------------------------------------------------------------------------

void serviceLed() {
  if (STATUS_LED_PIN < 0) return;
  bool on;
  if (net.connected()) {
    on = true;
  } else if (net.wifiConnected()) {
    on = (millis() / 500) % 2;
  } else {
    on = (millis() / 125) % 2;
  }
  digitalWrite(STATUS_LED_PIN, on ? HIGH : LOW);
}

void setupWatchdog() {
#if ESP_IDF_VERSION_MAJOR >= 5
  esp_task_wdt_config_t cfg = {};
  cfg.timeout_ms = TASK_WDT_TIMEOUT_S * 1000;
  cfg.trigger_panic = true;
  esp_task_wdt_reconfigure(&cfg);
#else
  esp_task_wdt_init(TASK_WDT_TIMEOUT_S, true);
#endif
  esp_task_wdt_add(nullptr);
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(50);
  log_i("12V Socket Detector fw %s", FW_VERSION);

  setupWatchdog();
  if (STATUS_LED_PIN >= 0) pinMode(STATUS_LED_PIN, OUTPUT);

  output.begin();
  output.onChange(onOutputChange);

  power.begin();
  power.onChange(onInputChange);
  currentSource = power.source();

  net.begin();
  net.onMessage(onCommand);
  net.onConnected(onMqttConnected);

  JsonDocument& boot = events.push("boot");
  boot["fw"] = FW_VERSION;
  boot["reset_reason"] = resetReasonStr(esp_reset_reason());
  boot["source"] = currentSource;
  fillInputs(boot["inputs"].to<JsonObject>());
}

void loop() {
  esp_task_wdt_reset();

  power.loop();
  checkBackupLow();
  output.loop();
  net.loop();
  serviceTelemetry();
  serviceLed();

  delay(5);  // yield; keeps Wi-Fi modem sleep effective
}
