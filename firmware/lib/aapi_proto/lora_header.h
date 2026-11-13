// LoRa packet header and the rover ESP32's per-packet decision.
// Reads the 5-byte header only, never the payload. 
#pragma once

#include <stddef.h>
#include <stdint.h>

#include "aapi_ids.h"

namespace aapi {

struct LoraHeader {
  uint8_t net_id;
  uint8_t type;
  uint8_t src;
  uint8_t dst;
  uint8_t seq;
};

// Returns false if len < LORA_HEADER_LEN (the caller drops the packet).
bool parse_header(const uint8_t* pkt, size_t len, LoraHeader* out);

// Who this board is. Set from CONFIG.
struct Identity {
  bool configured = false;
  uint8_t net_id = 0;
  uint8_t rover_id = 0;
};

enum class Action : uint8_t {
  DROP,     // not our network: don't forward
  STOP,     // stop the motors and latch, THEN forward it like FORWARD
  FORWARD,  // pass to the Pi as LORA_RX
};

// Not configured: any STOP counts (fail safe), everything else is forwarded.
// Configured: wrong net -> DROP; STOP for me or BROADCAST -> STOP; anything else -> FORWARD.
Action classify(const LoraHeader& h, const Identity& me);

}  // namespace aapi
