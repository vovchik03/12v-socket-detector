#include "output_control.h"

void OutputControl::begin() {
  pinMode(OUTPUT_PIN, OUTPUT);
  on_ = OUTPUT_DEFAULT_ON;
  digitalWrite(OUTPUT_PIN, (on_ == OUTPUT_ACTIVE_HIGH) ? HIGH : LOW);
}

void OutputControl::loop() {
  if (failsafeArmed_ && static_cast<int32_t>(millis() - failsafeAtMs_) >= 0) {
    apply(true, "failsafe");
  }
}

void OutputControl::turnOn(const char* reason) { apply(true, reason); }

void OutputControl::turnOff(const char* reason, uint32_t offForS) {
  if (OUTPUT_FAILSAFE_ENABLED) {
    if (offForS == 0) offForS = OUTPUT_FAILSAFE_DEFAULT_S;
    if (offForS > OUTPUT_FAILSAFE_MAX_S) offForS = OUTPUT_FAILSAFE_MAX_S;
    failsafeAtMs_ = millis() + offForS * 1000UL;
    failsafeArmed_ = true;
  }
  if (on_) {
    apply(false, reason);
  } else if (changeCb_) {
    changeCb_(false, reason);  // already off: report the re-armed timer
  }
}

uint32_t OutputControl::failsafeRemainingS() const {
  if (!failsafeArmed_) return 0;
  const int32_t left = static_cast<int32_t>(failsafeAtMs_ - millis());
  return left > 0 ? (left + 999) / 1000 : 0;
}

void OutputControl::apply(bool on, const char* reason) {
  if (on) failsafeArmed_ = false;
  if (on == on_) return;
  on_ = on;
  digitalWrite(OUTPUT_PIN, (on == OUTPUT_ACTIVE_HIGH) ? HIGH : LOW);
  if (changeCb_) changeCb_(on, reason);
}
