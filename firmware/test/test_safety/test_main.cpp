// Native tests for lib/safety StopLatch.
#include <unity.h>

#include "stop_latch.h"

using aapi::StopLatch;

void setUp() {}
void tearDown() {}

void test_new_latch_passes_speed_through() {
  StopLatch latch;
  TEST_ASSERT_FALSE(latch.active());
  TEST_ASSERT_EQUAL_INT16(50, latch.limit(50));
  TEST_ASSERT_EQUAL_INT16(-50, latch.limit(-50));
}

void test_tripped_latch_forces_zero_both_directions() {
  StopLatch latch;
  latch.trip(1);
  TEST_ASSERT_TRUE(latch.active());
  TEST_ASSERT_EQUAL_INT16(0, latch.limit(50));
  TEST_ASSERT_EQUAL_INT16(0, latch.limit(-50));
}

void test_clear_releases() {
  StopLatch latch;
  latch.trip(1);
  latch.clear();
  TEST_ASSERT_FALSE(latch.active());
  TEST_ASSERT_EQUAL_INT16(50, latch.limit(50));
  TEST_ASSERT_EQUAL_INT16(-50, latch.limit(-50));
}

void test_repeat_trip_keeps_first_reason() {
  StopLatch latch;
  latch.trip(1);
  latch.trip(2);  // repeated STOP while latched
  TEST_ASSERT_EQUAL_UINT8(1, latch.reason());
  latch.clear();
  latch.trip(3);  // a new stop after a clear
  TEST_ASSERT_EQUAL_UINT8(3, latch.reason());
}

void test_clear_on_new_latch_is_harmless() {
  StopLatch latch;
  latch.clear();
  TEST_ASSERT_FALSE(latch.active());
  TEST_ASSERT_EQUAL_INT16(50, latch.limit(50));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_new_latch_passes_speed_through);
  RUN_TEST(test_tripped_latch_forces_zero_both_directions);
  RUN_TEST(test_clear_releases);
  RUN_TEST(test_repeat_trip_keeps_first_reason);
  RUN_TEST(test_clear_on_new_latch_is_harmless);
  return UNITY_END();
}
