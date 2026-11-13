#include "mqtt_link.h"

#include <PubSubClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#include "config.h"

#if __has_include("secrets.h")
#include "secrets.h"
#else
#warning "include/secrets.h not found - using secrets.example.h placeholders"
#include "secrets.example.h"
#endif

namespace {

#if MQTT_USE_TLS
WiFiClientSecure net;
#else
WiFiClient net;
#endif
PubSubClient mqtt(net);

constexpr const char* STATUS_ONLINE = "online";
constexpr const char* STATUS_OFFLINE = "offline";
constexpr size_t MQTT_BUFFER_SIZE = 768;

String macSuffix() {
  uint8_t mac[6];
  WiFi.macAddress(mac);
  char buf[7];
  snprintf(buf, sizeof(buf), "%02x%02x%02x", mac[3], mac[4], mac[5]);
  return String(buf);
}

}  // namespace

void MqttLink::begin() {
  WiFi.mode(WIFI_STA);  // must precede macAddress()
  deviceId_ = strlen(DEVICE_ID) ? String(DEVICE_ID) : String("psd-") + macSuffix();
  base_ = String(MQTT_TOPIC_PREFIX) + "/" + deviceId_ + "/";

  WiFi.setHostname(deviceId_.c_str());
  WiFi.setAutoReconnect(true);
  WiFi.setSleep(true);  // modem sleep: lower power, slightly higher latency
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  lastWifiAttemptMs_ = millis();

#if MQTT_USE_TLS
  if (strstr(MQTT_CA_CERT, "BEGIN CERTIFICATE")) {
    net.setCACert(MQTT_CA_CERT);
  } else {
    log_w("MQTT_CA_CERT is empty: TLS without server verification (dev only)");
    net.setInsecure();
  }
#endif

  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setKeepAlive(MQTT_KEEPALIVE_S);
  mqtt.setSocketTimeout(5);
  mqtt.setBufferSize(MQTT_BUFFER_SIZE);
  mqtt.setCallback([this](char* topic, uint8_t* payload, unsigned int len) {
    if (!messageCb_) return;
    const size_t baseLen = base_.length();
    if (strncmp(topic, base_.c_str(), baseLen) != 0) return;
    String body;
    body.reserve(len);
    for (unsigned int i = 0; i < len; i++) body += static_cast<char>(payload[i]);
    messageCb_(topic + baseLen, body.c_str());
  });

  log_i("device id: %s, broker %s:%d (tls=%d)", deviceId_.c_str(), MQTT_HOST, MQTT_PORT,
        MQTT_USE_TLS);
}

bool MqttLink::wifiConnected() const { return WiFi.status() == WL_CONNECTED; }

bool MqttLink::connected() { return mqtt.connected(); }

int MqttLink::rssi() const { return wifiConnected() ? WiFi.RSSI() : 0; }

String MqttLink::topic(const char* subtopic) const { return base_ + subtopic; }

bool MqttLink::connectMqtt() {
  const String willTopic = topic("status");
  log_i("MQTT connecting...");
  const bool ok = mqtt.connect(deviceId_.c_str(), MQTT_USER, MQTT_PASSWORD, willTopic.c_str(),
                               1, true, STATUS_OFFLINE, true);
  if (!ok) {
    log_w("MQTT connect failed, state=%d", mqtt.state());
    return false;
  }
  mqtt.publish(willTopic.c_str(), STATUS_ONLINE, true);
  mqtt.subscribe(topic("cmd/#").c_str(), 1);
  log_i("MQTT connected");
  return true;
}

void MqttLink::loop() {
  const uint32_t now = millis();

  if (!wifiConnected()) {
    wasConnected_ = false;
    if (now - lastWifiAttemptMs_ >= WIFI_RECONNECT_MS) {
      lastWifiAttemptMs_ = now;
      log_w("Wi-Fi down, reconnecting");
      WiFi.reconnect();
    }
    return;
  }

  if (mqtt.connected()) {
    mqtt.loop();
    return;
  }

  if (wasConnected_) {
    log_w("MQTT connection lost");
    wasConnected_ = false;
  }
  if (now - lastMqttAttemptMs_ < mqttBackoffMs_) return;
  lastMqttAttemptMs_ = now;

  if (connectMqtt()) {
    wasConnected_ = true;
    mqttBackoffMs_ = 0;
    if (connectedCb_) connectedCb_();
  } else {
    mqttBackoffMs_ = mqttBackoffMs_ ? min(mqttBackoffMs_ * 2, MQTT_RECONNECT_MAX_MS)
                                    : MQTT_RECONNECT_MIN_MS;
  }
}

bool MqttLink::publish(const char* subtopic, const char* payload, bool retained) {
  if (!mqtt.connected()) return false;
  return mqtt.publish(topic(subtopic).c_str(), payload, retained);
}

bool MqttLink::publish(const char* subtopic, const JsonDocument& doc, bool retained) {
  if (!mqtt.connected()) return false;
  char buf[MQTT_BUFFER_SIZE - 128];  // leave room for topic + MQTT header
  const size_t n = serializeJson(doc, buf, sizeof(buf));
  if (n == 0 || n >= sizeof(buf) - 1) {
    log_e("payload for %s too large", subtopic);
    return false;
  }
  return mqtt.publish(topic(subtopic).c_str(), buf, retained);
}
