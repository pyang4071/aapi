#include "stop_latch.h"

namespace aapi {

void StopLatch::trip(uint8_t reason) {
  if (!active_) {
    reason_ = reason;  // the first cause is the one worth reporting
  }
  active_ = true;
}

void StopLatch::clear() { active_ = false; }

}  // namespace aapi
