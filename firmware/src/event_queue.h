#pragma once
// Fixed-size FIFO of events that are kept until the broker accepts them.
// When full, the oldest event is dropped (and counted).

#include <Arduino.h>
#include <ArduinoJson.h>
#include "config.h"

class EventQueue {
 public:
  struct Event {
    JsonDocument doc;
    uint32_t createdMs = 0;
  };

  // Returns the slot to fill; "type" and "seq" are already set.
  JsonDocument& push(const char* type) {
    if (count_ == EVENT_QUEUE_SIZE) {
      head_ = (head_ + 1) % EVENT_QUEUE_SIZE;
      count_--;
      dropped_++;
    }
    Event& e = items_[(head_ + count_) % EVENT_QUEUE_SIZE];
    count_++;
    e.doc.clear();
    e.doc["type"] = type;
    e.doc["seq"] = ++seq_;
    e.createdMs = millis();
    return e.doc;
  }

  bool empty() const { return count_ == 0; }
  Event& front() { return items_[head_]; }
  void pop() {
    if (!count_) return;
    items_[head_].doc.clear();
    head_ = (head_ + 1) % EVENT_QUEUE_SIZE;
    count_--;
  }
  uint32_t dropped() const { return dropped_; }

 private:
  Event items_[EVENT_QUEUE_SIZE];
  size_t head_ = 0;
  size_t count_ = 0;
  uint32_t seq_ = 0;
  uint32_t dropped_ = 0;
};
