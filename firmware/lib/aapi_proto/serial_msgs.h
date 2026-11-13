// Serial-link message payloads (architecture.md §5). Twin of src/aapi/protocol/serial_msgs.py.
// No Arduino, no heap.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include "aapi_ids.h"

namespace aapi {

// LORA_RX payload: rssi i16 (dBm) | snr i8 (0.25 dB steps) | packet. Little-endian.
// Writes into out and returns the payload length, or 0 if len > MAX_LORA_PACKET or out_cap is
// too small. out_cap >= MAX_PAYLOAD_LEN always fits.
size_t encode_lora_rx(int16_t rssi_dbm, int8_t snr_qdb, const uint8_t* pkt, size_t len,
                      uint8_t* out, size_t out_cap);

// RadioLib gives floats; these round and clamp them to the wire types.
int16_t rssi_to_dbm(float rssi_dbm);
int8_t snr_to_qdb(float snr_db);  // 7.5 dB -> 30

}  // namespace aapi
