#include <stdint.h>
#include <unity.h>

#include "companion_controller.h"

static void test_defaults_to_overview_and_ready_focus() {
  CompanionController controller;

  TEST_ASSERT_EQUAL_INT(static_cast<int>(Page::Overview), static_cast<int>(controller.page()));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(FocusPhase::Focus), static_cast<int>(controller.focusPhase()));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(FocusRunState::Ready), static_cast<int>(controller.focusRunState()));
  TEST_ASSERT_EQUAL_UINT32(25 * 60, controller.remainingSeconds());
}

static void test_short_press_cycles_three_pages() {
  CompanionController controller;

  controller.onShortPress();
  TEST_ASSERT_EQUAL_INT(static_cast<int>(Page::Activity), static_cast<int>(controller.page()));
  controller.onShortPress();
  TEST_ASSERT_EQUAL_INT(static_cast<int>(Page::Focus), static_cast<int>(controller.page()));
  controller.onShortPress();
  TEST_ASSERT_EQUAL_INT(static_cast<int>(Page::Overview), static_cast<int>(controller.page()));
}

static void test_double_press_starts_pauses_and_resumes() {
  CompanionController controller;

  controller.onDoublePress(1000);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(Page::Focus), static_cast<int>(controller.page()));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(FocusRunState::Running), static_cast<int>(controller.focusRunState()));
  controller.tick(61000);
  TEST_ASSERT_EQUAL_UINT32(24 * 60, controller.remainingSeconds());

  controller.onDoublePress(61000);
  controller.tick(121000);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(FocusRunState::Paused), static_cast<int>(controller.focusRunState()));
  TEST_ASSERT_EQUAL_UINT32(24 * 60, controller.remainingSeconds());

  controller.onDoublePress(121000);
  controller.tick(181000);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(FocusRunState::Running), static_cast<int>(controller.focusRunState()));
  TEST_ASSERT_EQUAL_UINT32(23 * 60, controller.remainingSeconds());
}

static void test_long_press_resets_focus() {
  CompanionController controller;
  controller.onDoublePress(0);
  controller.tick(60000);

  controller.onLongPress();

  TEST_ASSERT_EQUAL_INT(static_cast<int>(Page::Focus), static_cast<int>(controller.page()));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(FocusPhase::Focus), static_cast<int>(controller.focusPhase()));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(FocusRunState::Ready), static_cast<int>(controller.focusRunState()));
  TEST_ASSERT_EQUAL_UINT32(25 * 60, controller.remainingSeconds());
}

static void test_focus_and_break_completion_prepare_next_phase() {
  CompanionController controller;
  controller.onDoublePress(1000);
  controller.tick(1501000);

  TEST_ASSERT_EQUAL_INT(static_cast<int>(FocusPhase::Break), static_cast<int>(controller.focusPhase()));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(FocusRunState::Ready), static_cast<int>(controller.focusRunState()));
  TEST_ASSERT_EQUAL_UINT32(5 * 60, controller.remainingSeconds());
  TEST_ASSERT_TRUE(controller.consumeIntervalCompleted());
  TEST_ASSERT_FALSE(controller.consumeIntervalCompleted());

  controller.onDoublePress(1501000);
  controller.tick(1801000);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(FocusPhase::Focus), static_cast<int>(controller.focusPhase()));
  TEST_ASSERT_EQUAL_UINT32(25 * 60, controller.remainingSeconds());
}

static void test_timer_survives_millis_wraparound() {
  CompanionController controller;
  const uint32_t startedAt = UINT32_MAX - 29999;
  controller.onDoublePress(startedAt);

  controller.tick(30000);

  TEST_ASSERT_EQUAL_UINT32(24 * 60, controller.remainingSeconds());
}

int main(int argc, char **argv) {
  UNITY_BEGIN();
  RUN_TEST(test_defaults_to_overview_and_ready_focus);
  RUN_TEST(test_short_press_cycles_three_pages);
  RUN_TEST(test_double_press_starts_pauses_and_resumes);
  RUN_TEST(test_long_press_resets_focus);
  RUN_TEST(test_focus_and_break_completion_prepare_next_phase);
  RUN_TEST(test_timer_survives_millis_wraparound);
  return UNITY_END();
}
