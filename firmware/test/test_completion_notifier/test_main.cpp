#include <unity.h>

#include "completion_notifier.h"

static void test_first_snapshot_sets_baseline_without_playing() {
  CompletionNotifier notifier;

  TEST_ASSERT_FALSE(notifier.observe(7, 0));
}

static void test_new_completion_plays_exactly_once() {
  CompletionNotifier notifier;
  notifier.observe(7, 0);

  TEST_ASSERT_TRUE(notifier.observe(8, 1000));
  TEST_ASSERT_TRUE(notifier.shouldPlay(14, true, false));
  TEST_ASSERT_FALSE(notifier.observe(8, 1001));
}

static void test_quiet_hours_suppress_and_consume_completion() {
  CompletionNotifier notifier;
  notifier.observe(7, 0);

  TEST_ASSERT_TRUE(notifier.observe(8, 1000));
  TEST_ASSERT_FALSE(notifier.shouldPlay(23, true, false));
  TEST_ASSERT_FALSE(notifier.observe(8, 61000));
}

static void test_audio_always_overrides_quiet_hours() {
  CompletionNotifier notifier;
  notifier.observe(7, 0);

  TEST_ASSERT_TRUE(notifier.observe(8, 1000));
  TEST_ASSERT_TRUE(notifier.shouldPlay(23, true, true));
}

static void test_disabled_audio_never_replays_missed_completion() {
  CompletionNotifier notifier;
  notifier.observe(7, 0);

  TEST_ASSERT_TRUE(notifier.observe(8, 1000));
  TEST_ASSERT_FALSE(notifier.shouldPlay(14, false, false));
  TEST_ASSERT_FALSE(notifier.observe(8, 1001));
}

static void test_sequence_reset_rebaselines_without_replaying() {
  CompletionNotifier notifier;
  notifier.observe(7, 0);

  TEST_ASSERT_FALSE(notifier.observe(0, 1000));
  TEST_ASSERT_TRUE(notifier.observe(1, 2000));
}

static void test_celebration_expires_locally_before_next_sixty_second_snapshot() {
  CompletionNotifier notifier;
  notifier.observe(7, 0);

  TEST_ASSERT_TRUE(notifier.observe(8, 1000));
  TEST_ASSERT_TRUE(notifier.celebrating(30999));
  TEST_ASSERT_FALSE(notifier.celebrating(31000));
  TEST_ASSERT_FALSE(notifier.observe(8, 61000));
  TEST_ASSERT_FALSE(notifier.celebrating(61000));
}

static void test_celebration_deadline_survives_millis_wraparound() {
  CompletionNotifier notifier;
  notifier.observe(7, UINT32_MAX - 2000U);

  TEST_ASSERT_TRUE(notifier.observe(8, UINT32_MAX - 1000U));
  TEST_ASSERT_TRUE(notifier.celebrating(0));
  TEST_ASSERT_FALSE(notifier.celebrating(28999U));
}

int main(int argc, char **argv) {
  UNITY_BEGIN();
  RUN_TEST(test_first_snapshot_sets_baseline_without_playing);
  RUN_TEST(test_new_completion_plays_exactly_once);
  RUN_TEST(test_quiet_hours_suppress_and_consume_completion);
  RUN_TEST(test_audio_always_overrides_quiet_hours);
  RUN_TEST(test_disabled_audio_never_replays_missed_completion);
  RUN_TEST(test_sequence_reset_rebaselines_without_replaying);
  RUN_TEST(test_celebration_expires_locally_before_next_sixty_second_snapshot);
  RUN_TEST(test_celebration_deadline_survives_millis_wraparound);
  return UNITY_END();
}
