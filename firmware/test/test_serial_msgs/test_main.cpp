// Native tests for lib/aapi_proto serial_msgs: C++ must match serial_msgs.py byte for byte.
#include <unity.h>

#include "../golden_vectors.h"
#include "serial_msgs.h"

void setUp() {}
void tearDown() {}

void test_lora_rx_matches_python() {
  uint8_t out[aapi::MAX_PAYLOAD_LEN];
  size_t n = aapi::encode_lora_rx(GOLDEN_LORA_RX_RSSI, GOLDEN_LORA_RX_SNR_QDB,
                                  GOLDEN_LORA_RX_PACKET, GOLDEN_LORA_RX_PACKET_LEN, out,
                                  sizeof(out));
  TEST_ASSERT_EQUAL_size_t(GOLDEN_LORA_RX_PAYLOAD_LEN, n);
  TEST_ASSERT_EQUAL_HEX8_ARRAY(GOLDEN_LORA_RX_PAYLOAD, out, n);
}

void test_lora_rx_rejects_oversize_packet() {
  static uint8_t pkt[aapi::MAX_LORA_PACKET + 1];
  uint8_t out[aapi::MAX_PAYLOAD_LEN + 8];
  TEST_ASSERT_EQUAL_size_t(0, aapi::encode_lora_rx(0, 0, pkt, sizeof(pkt), out, sizeof(out)));
}

void test_snr_rounds_to_quarter_db_and_clamps() {
  TEST_ASSERT_EQUAL_INT8(30, aapi::snr_to_qdb(7.4f));  // 29.6 quarter-dB -> 30
  TEST_ASSERT_EQUAL_INT8(-20, aapi::snr_to_qdb(-5.0f));
  TEST_ASSERT_EQUAL_INT8(INT8_MAX, aapi::snr_to_qdb(100.0f));
  TEST_ASSERT_EQUAL_INT8(INT8_MIN, aapi::snr_to_qdb(-100.0f));
}

void test_rssi_rounds_and_clamps() {
  TEST_ASSERT_EQUAL_INT16(-87, aapi::rssi_to_dbm(-86.6f));
  TEST_ASSERT_EQUAL_INT16(INT16_MIN, aapi::rssi_to_dbm(-1e6f));
  TEST_ASSERT_EQUAL_INT16(INT16_MAX, aapi::rssi_to_dbm(1e6f));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_lora_rx_matches_python);
  RUN_TEST(test_lora_rx_rejects_oversize_packet);
  RUN_TEST(test_snr_rounds_to_quarter_db_and_clamps);
  RUN_TEST(test_rssi_rounds_and_clamps);
  return UNITY_END();
}
