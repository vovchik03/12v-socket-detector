#pragma once
// Samples the 12V input channels from config.h, filters them and reports
// debounced presence changes.

#include <Arduino.h>
#include "config.h"

class PowerMonitor {
 public:
  using ChangeCallback = void (*)(size_t channel, bool present, float volts);

  void begin();
  void loop();
  void onChange(ChangeCallback cb) { changeCb_ = cb; }

  size_t count() const { return INPUT_CHANNEL_COUNT; }
  const InputChannelConfig& config(size_t i) const { return INPUT_CHANNELS[i]; }
  float volts(size_t i) const { return state_[i].volts; }
  bool present(size_t i) const { return state_[i].present; }

  // Index of the first channel with this role, or -1.
  int indexOf(InputRole role) const;

  // "main" when grid power is present, "backup" when running from a backup
  // input, "none" otherwise (e.g. powered over USB on the bench).
  const char* source() const;

 private:
  struct ChannelState {
    float volts = 0;
    bool present = false;
    bool pending = false;       // a change is being debounced
    uint32_t pendingSince = 0;
  };

  float readVolts(size_t i) const;

  ChannelState state_[INPUT_CHANNEL_COUNT];
  uint32_t lastSampleMs_ = 0;
  ChangeCallback changeCb_ = nullptr;
};
