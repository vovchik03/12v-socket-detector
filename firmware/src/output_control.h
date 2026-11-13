#pragma once
// 12V load switch with a failsafe: since the router hangs on this output,
// "off" is always temporary and the output turns itself back on.

#include <Arduino.h>
#include "config.h"

class OutputControl {
 public:
  using ChangeCallback = void (*)(bool on, const char* reason);

  void begin();
  void loop();
  void onChange(ChangeCallback cb) { changeCb_ = cb; }

  void turnOn(const char* reason);
  // offForS == 0 -> OUTPUT_FAILSAFE_DEFAULT_S (when failsafe is enabled).
  void turnOff(const char* reason, uint32_t offForS = 0);

  bool isOn() const { return on_; }
  // Seconds until the failsafe turns the output back on, 0 if not armed.
  uint32_t failsafeRemainingS() const;

 private:
  void apply(bool on, const char* reason);

  bool on_ = false;
  bool failsafeArmed_ = false;
  uint32_t failsafeAtMs_ = 0;
  ChangeCallback changeCb_ = nullptr;
};
