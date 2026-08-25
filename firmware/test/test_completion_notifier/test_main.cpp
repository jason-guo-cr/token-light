#include <unity.h>

#include "completion_notifier.h"

static void test_first_snapshot_sets_baseline_without_playing() {
  CompletionNotifier notifier;

  TEST_ASSERT_FALSE(notifier.shouldPlay(7, 14, true, false));
}

static void test_new_completion_plays_exactly_once() {
  CompletionNotifier notifier;
  notifier.shouldPlay(7, 14, true, false);

  TEST_ASSERT_TRUE(notifier.shouldPlay(8, 14, true, false));
  TEST_ASSERT_FALSE(notifier.shouldPlay(8, 14, true, false));
}

static void test_quiet_hours_suppress_and_consume_completion() {
  CompletionNotifier notifier;
  notifier.shouldPlay(7, 21, true, false);

  TEST_ASSERT_FALSE(notifier.shouldPlay(8, 23, true, false));
  TEST_ASSERT_FALSE(notifier.shouldPlay(8, 8, true, false));
}

static void test_audio_always_overrides_quiet_hours() {
  CompletionNotifier notifier;
  notifier.shouldPlay(7, 21, true, true);

  TEST_ASSERT_TRUE(notifier.shouldPlay(8, 23, true, true));
}

static void test_disabled_audio_never_replays_missed_completion() {
  CompletionNotifier notifier;
  notifier.shouldPlay(7, 14, false, false);

  TEST_ASSERT_FALSE(notifier.shouldPlay(8, 14, false, false));
  TEST_ASSERT_FALSE(notifier.shouldPlay(8, 14, true, false));
}

int main(int argc, char **argv) {
  UNITY_BEGIN();
  RUN_TEST(test_first_snapshot_sets_baseline_without_playing);
  RUN_TEST(test_new_completion_plays_exactly_once);
  RUN_TEST(test_quiet_hours_suppress_and_consume_completion);
  RUN_TEST(test_audio_always_overrides_quiet_hours);
  RUN_TEST(test_disabled_audio_never_replays_missed_completion);
  return UNITY_END();
}
