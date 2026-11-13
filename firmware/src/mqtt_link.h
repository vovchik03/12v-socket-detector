#pragma once
// Wi-Fi + MQTT connection management with Last Will ("offline"), automatic
// reconnect with backoff and command subscription (psd/<id>/cmd/#).

#include <Arduino.h>
#include <ArduinoJson.h>

class MqttLink {
 public:
  // subtopic is relative to the device base, e.g. "cmd/out".
  using MessageCallback = void (*)(const char* subtopic, const char* payload);
  using ConnectedCallback = void (*)();

  void begin();
  void loop();
  void onMessage(MessageCallback cb) { messageCb_ = cb; }
  void onConnected(ConnectedCallback cb) { connectedCb_ = cb; }

  bool wifiConnected() const;
  bool connected();
  int rssi() const;

  bool publish(const char* subtopic, const char* payload, bool retained = false);
  bool publish(const char* subtopic, const JsonDocument& doc, bool retained = false);

  const String& deviceId() const { return deviceId_; }

 private:
  bool connectMqtt();
  String topic(const char* subtopic) const;

  String deviceId_;
  String base_;  // "psd/<device_id>/"
  uint32_t lastWifiAttemptMs_ = 0;
  uint32_t lastMqttAttemptMs_ = 0;
  uint32_t mqttBackoffMs_ = 0;
  bool wasConnected_ = false;
  MessageCallback messageCb_ = nullptr;
  ConnectedCallback connectedCb_ = nullptr;
};
