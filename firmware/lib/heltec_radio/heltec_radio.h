// Heltec WiFi LoRa 32 V3 radio (SX1262) behind three calls. 
// Unverified until checked on the real board: pin numbers and TCXO 1.8 V (heltec_radio.cpp).
#pragma once

#include <stddef.h>
#include <stdint.h>

#include "aapi_ids.h"

namespace aapi {

// LoRa settings; range test and CONFIG will change them. 
// Sync word and TCXO voltage are fixed in the .cpp.
struct RadioConfig {
  float freq_mhz = 915.0;
  float bw_khz = 500.0;
  uint8_t sf = 9;
  uint8_t cr = 5;  // RadioLib writes 4/5 as 5
  int8_t power_dbm = 14;
  uint16_t preamble = 8;
};

// One received LoRa packet, with its signal readings already in the LORA_RX wire types.
struct RadioPacket {
  uint8_t data[MAX_LORA_PACKET];
  size_t len;
  int16_t rssi_dbm;  // rssi_to_dbm()
  int8_t snr_qdb;    // snr_to_qdb(), 0.25 dB steps
};

// Start the radio and begin listening. False if the radio doesn't answer (wrong pins or TCXO).
bool radio_begin(const RadioConfig& cfg = RadioConfig{});

// Transmit one raw LoRa packet. Blocks until sent (packets arriving meanwhile are lost), then
// goes back to receive. False if len is 0 or > MAX_LORA_PACKET, or the radio reports an error.
bool radio_send(const uint8_t* pkt, size_t len);

// If a packet arrived since the last call, fill out and return true. False if nothing arrived or
// it was bad (radio CRC, bad length). Always leaves the radio listening.
bool radio_poll(RadioPacket& out);

}  // namespace aapi
