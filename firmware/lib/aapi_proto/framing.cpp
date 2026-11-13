#include "framing.h"

namespace aapi {

uint16_t crc16(const uint8_t* data, size_t len) {
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < len; ++i) {
    crc ^= static_cast<uint16_t>(data[i]) << 8;
    for (int bit = 0; bit < 8; ++bit) {
      crc = (crc & 0x8000) ? static_cast<uint16_t>((crc << 1) ^ 0x1021)
                           : static_cast<uint16_t>(crc << 1);
    }
  }
  return crc;
}

size_t cobs_encode(const uint8_t* in, size_t len, uint8_t* out) {
  size_t code_at = 0;  // where the current block's count byte goes
  size_t o = 1;
  uint8_t code = 1;  // block length + 1
  for (size_t i = 0; i < len; ++i) {
    if (in[i] == 0) {  // count byte stands in for the zero
      out[code_at] = code;
      code_at = o++;
      code = 1;
    } else {
      out[o++] = in[i];
      if (++code == 0xFF) {  // 254 bytes, no zero after: a count byte can't go higher
        out[code_at] = code;
        if (i == len - 1) {
          return o;  // data ended exactly on a full block: no trailing count byte
        }
        code_at = o++;
        code = 1;
      }
    }
  }
  out[code_at] = code;  // last block (no zero after it)
  return o;
}

bool cobs_decode(const uint8_t* in, size_t len, uint8_t* out, size_t* out_len) {
  size_t i = 0;
  size_t o = 0;
  while (i < len) {
    uint8_t code = in[i];
    if (code == 0) {
      return false;
    }
    size_t end = i + code;
    if (end > len) {
      return false;  // block runs past the end of the frame
    }
    for (size_t j = i + 1; j < end; ++j) {
      if (in[j] == 0) {
        return false;
      }
      out[o++] = in[j];
    }
    i = end;
    if (code != 0xFF && i < len) {
      out[o++] = 0;  // put the zero back (not after the last block)
    }
  }
  *out_len = o;
  return true;
}

size_t encode_frame(uint8_t type, const uint8_t* payload, size_t len, uint8_t* out,
                    size_t out_cap) {
  if (len > MAX_PAYLOAD_LEN) {
    return 0;
  }
  size_t body_len = 1 + len + CRC_LEN;
  if (out_cap < body_len + body_len / 254 + 2) {  // COBS overhead + 0x00 marker
    return 0;
  }
  uint8_t body[MAX_BODY_LEN];
  body[0] = type;
  for (size_t i = 0; i < len; ++i) {
    body[1 + i] = payload[i];
  }
  uint16_t crc = crc16(body, 1 + len);
  body[1 + len] = static_cast<uint8_t>(crc & 0xFF);  // little-endian
  body[2 + len] = static_cast<uint8_t>(crc >> 8);

  size_t n = cobs_encode(body, body_len, out);
  out[n++] = 0x00;
  return n;
}

bool FrameDecoder::feed(uint8_t byte) {
  if (byte == 0) {  // end of a frame
    bool good = false;
    if (len_ > 0 && !discarding_) {
      good = decode_buffered();
      if (!good) {
        ++bad_frames;  // drop it, keep going
      }
    }
    len_ = 0;
    discarding_ = false;
    return good;
  }
  if (discarding_) {
    return false;
  }
  if (len_ == MAX_ENCODED_LEN) {  // garbage with no 0x00: give up on it
    len_ = 0;
    discarding_ = true;
    ++bad_frames;
    return false;
  }
  buf_[len_++] = byte;
  return false;
}

bool FrameDecoder::decode_buffered() {
  size_t body_len = 0;
  if (!cobs_decode(buf_, len_, body_, &body_len)) {
    return false;
  }
  if (body_len < 1 + CRC_LEN) {
    return false;
  }
  size_t data_len = body_len - CRC_LEN;
  uint16_t received = static_cast<uint16_t>(body_[data_len] | (body_[data_len + 1] << 8));
  if (crc16(body_, data_len) != received) {
    return false;
  }
  payload_len_ = data_len - 1;
  return true;
}

}  // namespace aapi
