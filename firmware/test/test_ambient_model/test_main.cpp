#include <math.h>
#include <stdint.h>
#include <unity.h>

#include "ambient_model.h"

static void test_starts_without_a_valid_reading() {
  AmbientModel model(300000);
  AmbientSnapshot snapshot = model.snapshot(0);

  TEST_ASSERT_FALSE(snapshot.valid);
  TEST_ASSERT_FALSE(snapshot.cached);
}

static void test_accepts_and_rounds_valid_sample() {
  AmbientModel model(300000);

  TEST_ASSERT_TRUE(model.acceptSample(24.6f, 48.4f, 1000));
  AmbientSnapshot snapshot = model.snapshot(1000);

  TEST_ASSERT_TRUE(snapshot.valid);
  TEST_ASSERT_FALSE(snapshot.cached);
  TEST_ASSERT_EQUAL_INT(25, snapshot.temperatureC);
  TEST_ASSERT_EQUAL_INT(48, snapshot.humidityPercent);
}

static void test_rejects_nan_and_out_of_range_samples() {
  AmbientModel model(300000);

  TEST_ASSERT_FALSE(model.acceptSample(NAN, 40.0f, 1000));
  TEST_ASSERT_FALSE(model.acceptSample(20.0f, NAN, 1000));
  TEST_ASSERT_FALSE(model.acceptSample(-40.1f, 40.0f, 1000));
  TEST_ASSERT_FALSE(model.acceptSample(20.0f, 100.1f, 1000));
}

static void test_failed_read_keeps_cache_then_becomes_stale() {
  AmbientModel model(300000);
  model.acceptSample(24.6f, 48.4f, 1000);

  model.markReadFailed(61000);
  AmbientSnapshot cached = model.snapshot(61000);
  AmbientSnapshot stale = model.snapshot(301001);

  TEST_ASSERT_TRUE(cached.valid);
  TEST_ASSERT_TRUE(cached.cached);
  TEST_ASSERT_EQUAL_INT(25, cached.temperatureC);
  TEST_ASSERT_FALSE(stale.valid);
}

int main(int argc, char **argv) {
  UNITY_BEGIN();
  RUN_TEST(test_starts_without_a_valid_reading);
  RUN_TEST(test_accepts_and_rounds_valid_sample);
  RUN_TEST(test_rejects_nan_and_out_of_range_samples);
  RUN_TEST(test_failed_read_keeps_cache_then_becomes_stale);
  return UNITY_END();
}
