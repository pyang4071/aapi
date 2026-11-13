// Native tests for lib/aapi_proto framing: C++ must match framing.py byte for byte.
#include <string.h>
#include <unity.h>

#include "../golden_vectors.h"
#include "framing.h"

using aapi::FrameDecoder;

void setUp() {}
void tearDown() {}

// Feed a whole byte string; return how many good frames came out.
static int feed_all(FrameDecoder& dec, const uint8_t* data, size_t len) {
  int frames = 0;
  for (size_t i = 0; i < len; ++i) {
    if (dec.feed(data[i])) {
      ++frames;
    }
  }
  return frames;
}

void test_crc16_check_value() {
  const uint8_t check[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
  TEST_ASSERT_EQUAL_HEX16(0x29B1, aapi::crc16(check, sizeof(check)));
}

void test_encode_matches_python() {
  for (size_t i = 0; i < GOLDEN_COUNT; ++i) {
    const GoldenVector& v = GOLDEN_VECTORS[i];
    uint8_t out[aapi::MAX_FRAME_LEN];
    size_t n = aapi::encode_frame(v.type, v.payload, v.payload_len, out, sizeof(out));
    TEST_ASSERT_EQUAL_size_t_MESSAGE(v.frame_len, n, "frame length");
    TEST_ASSERT_EQUAL_HEX8_ARRAY(v.frame, out, v.frame_len);
  }
}

void test_decoder_reads_python_frames() {
  for (size_t i = 0; i < GOLDEN_COUNT; ++i) {
    const GoldenVector& v = GOLDEN_VECTORS[i];
    FrameDecoder dec;
    TEST_ASSERT_EQUAL_INT(1, feed_all(dec, v.frame, v.frame_len));
    TEST_ASSERT_EQUAL_HEX8(v.type, dec.type());
    TEST_ASSERT_EQUAL_size_t(v.payload_len, dec.payload_len());
    if (v.payload_len > 0) {
      TEST_ASSERT_EQUAL_HEX8_ARRAY(v.payload, dec.payload(), v.payload_len);
    }
    TEST_ASSERT_EQUAL_UINT32(0, dec.bad_frames);
  }
}

void test_corrupt_frame_dropped_then_recovers() {
  const GoldenVector& v = GOLDEN_VECTORS[1];
  uint8_t bad[aapi::MAX_FRAME_LEN];
  memcpy(bad, v.frame, v.frame_len);
  bad[v.frame_len - 2] ^= 0x01;  // flip a CRC bit (stays non-zero, so framing survives)
  if (bad[v.frame_len - 2] == 0) {
    bad[v.frame_len - 2] = 0x02;
  }

  FrameDecoder dec;
  TEST_ASSERT_EQUAL_INT(0, feed_all(dec, bad, v.frame_len));
  TEST_ASSERT_EQUAL_UINT32(1, dec.bad_frames);
  TEST_ASSERT_EQUAL_INT(1, feed_all(dec, v.frame, v.frame_len));
  TEST_ASSERT_EQUAL_HEX8(v.type, dec.type());
}

void test_overlong_garbage_dropped_then_recovers() {
  FrameDecoder dec;
  for (size_t i = 0; i < aapi::MAX_ENCODED_LEN + 10; ++i) {
    TEST_ASSERT_FALSE(dec.feed(0x55));
  }
  TEST_ASSERT_EQUAL_UINT32(1, dec.bad_frames);
  TEST_ASSERT_FALSE(dec.feed(0x00));  // ends the discarded frame, not a frame of its own
  TEST_ASSERT_EQUAL_UINT32(1, dec.bad_frames);

  const GoldenVector& v = GOLDEN_VECTORS[0];
  TEST_ASSERT_EQUAL_INT(1, feed_all(dec, v.frame, v.frame_len));
}

void test_encode_rejects_oversize_payload() {
  static uint8_t payload[aapi::MAX_PAYLOAD_LEN + 1];
  uint8_t out[aapi::MAX_FRAME_LEN + 8];
  TEST_ASSERT_EQUAL_size_t(
      0, aapi::encode_frame(0x01, payload, sizeof(payload), out, sizeof(out)));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_crc16_check_value);
  RUN_TEST(test_encode_matches_python);
  RUN_TEST(test_decoder_reads_python_frames);
  RUN_TEST(test_corrupt_frame_dropped_then_recovers);
  RUN_TEST(test_overlong_garbage_dropped_then_recovers);
  RUN_TEST(test_encode_rejects_oversize_payload);
  return UNITY_END();
}
