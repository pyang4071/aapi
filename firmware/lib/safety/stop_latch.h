// STOP latch
// Once tripped by a LoRa STOP, every speed command is forced to 0 until the rover Pi sends
// STOP_CLEAR. Only clear() releases it.
#pragma once

#include <stdint.h>

namespace aapi {

class StopLatch {
 public:
  // STOP received. A repeat while already active keeps the first reason.
  void trip(uint8_t reason);
  // STOP_CLEAR from the Pi. Harmless if not active.
  void clear();

  bool active() const { return active_; }
  // Reason of the trip that set the latch (0 if never tripped).
  uint8_t reason() const { return reason_; }

  // Every DRIVE speed goes through here: 0 while active, unchanged otherwise.
  int16_t limit(int16_t requested_speed) const { return active_ ? 0 : requested_speed; }

 private:
  bool active_ = false;
  uint8_t reason_ = 0;
};

}  // namespace aapi
