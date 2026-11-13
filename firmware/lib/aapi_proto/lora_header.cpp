#include "lora_header.h"

namespace aapi {

bool parse_header(const uint8_t* pkt, size_t len, LoraHeader* out) {
  if (len < LORA_HEADER_LEN) {
    return false;
  }
  out->net_id = pkt[0];
  out->type = pkt[1];
  out->src = pkt[2];
  out->dst = pkt[3];
  out->seq = pkt[4];
  return true;
}

Action classify(const LoraHeader& h, const Identity& me) {
  bool is_stop = h.type == lora::STOP;
  if (!me.configured) {
    return is_stop ? Action::STOP : Action::FORWARD;  // don't know who we are: any STOP counts
  }
  if (h.net_id != me.net_id) {
    return Action::DROP;
  }
  if (is_stop && (h.dst == me.rover_id || h.dst == addr::BROADCAST)) {
    return Action::STOP;
  }
  return Action::FORWARD;  // includes a STOP for another rover: the Pi still sees it
}

}  // namespace aapi
