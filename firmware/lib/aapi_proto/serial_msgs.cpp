#include "serial_msgs.h"

#include <math.h>

namespace aapi {

size_t encode_lora_rx(int16_t rssi_dbm, int8_t snr_qdb, const uint8_t* pkt, size_t len,
                      uint8_t* out, size_t out_cap) {
  if (len > MAX_LORA_PACKET || out_cap < LORA_RX_HEADER_LEN + len) {
    return 0;
  }
  uint16_t rssi = static_cast<uint16_t>(rssi_dbm);
  out[0] = static_cast<uint8_t>(rssi & 0xFF);  // little-endian
  out[1] = static_cast<uint8_t>(rssi >> 8);
  out[2] = static_cast<uint8_t>(snr_qdb);
  for (size_t i = 0; i < len; ++i) {
    out[LORA_RX_HEADER_LEN + i] = pkt[i];
  }
  return LORA_RX_HEADER_LEN + len;
}

// Round to the nearest whole number, then clamp into [lo, hi].
static long round_clamp(float x, long lo, long hi) {
  if (isnan(x)) {
    return 0;
  }
  float r = roundf(x);
  if (r < static_cast<float>(lo)) {
    return lo;
  }
  if (r > static_cast<float>(hi)) {
    return hi;
  }
  return static_cast<long>(r);
}

int16_t rssi_to_dbm(float rssi_dbm) {
  return static_cast<int16_t>(round_clamp(rssi_dbm, INT16_MIN, INT16_MAX));
}

int8_t snr_to_qdb(float snr_db) {
  return static_cast<int8_t>(round_clamp(snr_db * 4.0f, INT8_MIN, INT8_MAX));
}

}  // namespace aapi
