// Serial-link framing (architecture.md §5): COBS + CRC-16/CCITT-FALSE.
// Byte-for-byte twin of src/aapi/protocol/framing.py. No Arduino, no heap.
// Frame on the wire: COBS(type | payload | crc16 little-endian) then 0x00.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include "aapi_ids.h"

namespace aapi {

// Biggest frame encode_frame() can produce, including the 0x00 end marker.
constexpr size_t MAX_FRAME_LEN = MAX_ENCODED_LEN + 1;

// CRC-16/CCITT-FALSE (poly 0x1021, init 0xFFFF), same as binascii.crc_hqx(data, 0xFFFF).
uint16_t crc16(const uint8_t* data, size_t len);

// COBS-encode len bytes into out (room for len + len / 254 + 1 bytes). Returns bytes written.
size_t cobs_encode(const uint8_t* in, size_t len, uint8_t* out);

// COBS-decode (0x00 marker already removed) into out (room for len bytes).
// Returns false on bad encoding.
bool cobs_decode(const uint8_t* in, size_t len, uint8_t* out, size_t* out_len);

// Build a full frame (ending in 0x00) into out. Returns its length, or 0 if the payload is
// longer than MAX_PAYLOAD_LEN or out_cap is too small. out_cap >= MAX_FRAME_LEN always fits.
size_t encode_frame(uint8_t type, const uint8_t* payload, size_t len, uint8_t* out,
                    size_t out_cap);

// Splits a serial byte stream into frames, one byte at a time.
//   while (Serial.available()) if (dec.feed(Serial.read())) handle(dec.type(), dec.payload(), dec.payload_len());
// payload() points into the decoder and is valid until the next feed().
class FrameDecoder {
 public:
  bool feed(uint8_t byte);  // true when a frame with a good CRC just finished

  uint8_t type() const { return body_[0]; }
  const uint8_t* payload() const { return body_ + 1; }
  size_t payload_len() const { return payload_len_; }

  uint32_t bad_frames = 0;  // bad COBS, too short, bad CRC, or too long without a 0x00

 private:
  uint8_t buf_[MAX_ENCODED_LEN];   // encoded bytes since the last 0x00
  uint8_t body_[MAX_ENCODED_LEN];  // decoded body of the last good frame
  size_t len_ = 0;
  size_t payload_len_ = 0;
  bool discarding_ = false;  // true after an overlong frame, until the next 0x00

  bool decode_buffered();
};

}  // namespace aapi
