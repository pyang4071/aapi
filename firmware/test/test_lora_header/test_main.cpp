// Native tests for lib/aapi_proto lora_header: parse the golden packets built by messages.py,
// then check classify() for each case the rover ESP32 must handle.
#include <string.h>
#include <unity.h>

#include "../golden_vectors.h"
#include "lora_header.h"

using aapi::Action;
using aapi::Identity;
using aapi::LoraHeader;

void setUp() {}
void tearDown() {}

static const GoldenPacket& golden(const char* label) {
  for (size_t i = 0; i < GOLDEN_PACKET_COUNT; ++i) {
    if (strcmp(GOLDEN_PACKETS[i].label, label) == 0) {
      return GOLDEN_PACKETS[i];
    }
  }
  TEST_FAIL_MESSAGE(label);  // no such golden packet: regenerate golden_vectors.h
  return GOLDEN_PACKETS[0];
}

static LoraHeader parsed(const GoldenPacket& g) {
  LoraHeader h{};
  TEST_ASSERT_TRUE_MESSAGE(aapi::parse_header(g.bytes, g.len, &h), g.label);
  return h;
}

static Identity rover(uint8_t id) {
  Identity me;
  me.configured = true;
  me.net_id = GOLDEN_NET_ID;
  me.rover_id = id;
  return me;
}

static Action classify_golden(const char* label, const Identity& me) {
  return aapi::classify(parsed(golden(label)), me);
}

void test_parse_matches_python() {
  for (size_t i = 0; i < GOLDEN_PACKET_COUNT; ++i) {
    const GoldenPacket& g = GOLDEN_PACKETS[i];
    LoraHeader h = parsed(g);
    TEST_ASSERT_EQUAL_HEX8_MESSAGE(g.net_id, h.net_id, g.label);
    TEST_ASSERT_EQUAL_HEX8_MESSAGE(g.type, h.type, g.label);
    TEST_ASSERT_EQUAL_HEX8_MESSAGE(g.src, h.src, g.label);
    TEST_ASSERT_EQUAL_HEX8_MESSAGE(g.dst, h.dst, g.label);
    TEST_ASSERT_EQUAL_HEX8_MESSAGE(g.seq, h.seq, g.label);
  }
}

void test_parse_rejects_short_packet() {
  const GoldenPacket& g = golden("resume_rover3");  // exactly a header, no payload
  LoraHeader h{};
  TEST_ASSERT_TRUE(aapi::parse_header(g.bytes, aapi::LORA_HEADER_LEN, &h));
  TEST_ASSERT_FALSE(aapi::parse_header(g.bytes, aapi::LORA_HEADER_LEN - 1, &h));
}

void test_classify_as_the_addressed_rover() {
  Identity me = rover(3);
  TEST_ASSERT_EQUAL(Action::STOP, classify_golden("stop_rover3", me));
  TEST_ASSERT_EQUAL(Action::STOP, classify_golden("stop_all", me));
  TEST_ASSERT_EQUAL(Action::FORWARD, classify_golden("resume_rover3", me));
  TEST_ASSERT_EQUAL(Action::FORWARD, classify_golden("text_from_rover3", me));
}

void test_classify_as_another_rover() {
  Identity me = rover(4);
  TEST_ASSERT_EQUAL(Action::FORWARD, classify_golden("stop_rover3", me));
  TEST_ASSERT_EQUAL(Action::STOP, classify_golden("stop_all", me));
}

void test_classify_drops_other_network() {
  LoraHeader h = parsed(golden("stop_all"));
  h.net_id = static_cast<uint8_t>(GOLDEN_NET_ID ^ 0x01);
  TEST_ASSERT_EQUAL(Action::DROP, aapi::classify(h, rover(3)));
}

void test_classify_unconfigured_fails_safe() {
  Identity unknown;  // no CONFIG yet
  LoraHeader h = parsed(golden("stop_rover3"));
  h.net_id = static_cast<uint8_t>(GOLDEN_NET_ID ^ 0x01);  // any network
  TEST_ASSERT_EQUAL(Action::STOP, aapi::classify(h, unknown));
  TEST_ASSERT_EQUAL(Action::FORWARD, classify_golden("text_from_rover3", unknown));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_parse_matches_python);
  RUN_TEST(test_parse_rejects_short_packet);
  RUN_TEST(test_classify_as_the_addressed_rover);
  RUN_TEST(test_classify_as_another_rover);
  RUN_TEST(test_classify_drops_other_network);
  RUN_TEST(test_classify_unconfigured_fails_safe);
  return UNITY_END();
}
