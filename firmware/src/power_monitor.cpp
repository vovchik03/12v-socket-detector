#include "power_monitor.h"

void PowerMonitor::begin() {
  analogReadResolution(12);
  for (size_t i = 0; i < INPUT_CHANNEL_COUNT; i++) {
    analogSetPinAttenuation(INPUT_CHANNELS[i].pin, ADC_11db);  // ~0..3.1V range
  }

  // Seed the filter with an averaged reading and take the initial state as-is
  // (no change events at boot — the boot event carries the initial state).
  for (size_t i = 0; i < INPUT_CHANNEL_COUNT; i++) {
    float sum = 0;
    for (int n = 0; n < 8; n++) {
      sum += readVolts(i);
      delay(2);
    }
    ChannelState& s = state_[i];
    s.volts = sum / 8;
    s.present = s.volts >= INPUT_CHANNELS[i].presentOnV;
    s.pending = false;
  }
  lastSampleMs_ = millis();
}

float PowerMonitor::readVolts(size_t i) const {
  const InputChannelConfig& c = INPUT_CHANNELS[i];
  // analogReadMilliVolts() applies the factory eFuse ADC calibration.
  const float adcV = analogReadMilliVolts(c.pin) / 1000.0f;
  return adcV * c.dividerRatio * c.calibration;
}

void PowerMonitor::loop() {
  const uint32_t now = millis();
  if (now - lastSampleMs_ < INPUT_SAMPLE_INTERVAL_MS) return;
  lastSampleMs_ = now;

  for (size_t i = 0; i < INPUT_CHANNEL_COUNT; i++) {
    const InputChannelConfig& c = INPUT_CHANNELS[i];
    ChannelState& s = state_[i];

    s.volts += INPUT_FILTER_ALPHA * (readVolts(i) - s.volts);

    // Hysteresis: the threshold depends on the current confirmed state.
    const bool candidate = s.present ? (s.volts >= c.presentOffV) : (s.volts >= c.presentOnV);

    if (candidate == s.present) {
      s.pending = false;
      continue;
    }
    if (!s.pending) {
      s.pending = true;
      s.pendingSince = now;
      continue;
    }
    if (now - s.pendingSince >= INPUT_DEBOUNCE_MS) {
      s.present = candidate;
      s.pending = false;
      if (changeCb_) changeCb_(i, s.present, s.volts);
    }
  }
}

int PowerMonitor::indexOf(InputRole role) const {
  for (size_t i = 0; i < INPUT_CHANNEL_COUNT; i++) {
    if (INPUT_CHANNELS[i].role == role) return static_cast<int>(i);
  }
  return -1;
}

const char* PowerMonitor::source() const {
  const int main = indexOf(InputRole::Main);
  if (main >= 0 && state_[main].present) return "main";
  for (size_t i = 0; i < INPUT_CHANNEL_COUNT; i++) {
    if (INPUT_CHANNELS[i].role == InputRole::Backup && state_[i].present) return "backup";
  }
  return "none";
}
