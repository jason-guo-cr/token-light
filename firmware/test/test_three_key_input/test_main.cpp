#include <stdint.h>
#include <unity.h>

#include "board_input_config.h"
#include "companion_controller.h"
#include "key_gesture.h"
#include "three_key_input.h"

namespace {
ThreeKeyLevels released() { return {false, false, false}; }

ThreeKeyEvent updateTwice(
    ThreeKeyInput &input, const ThreeKeyLevels &levels, uint32_t changedAtMs) {
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(ThreeKeyEvent::None), static_cast<int>(input.update(levels, changedAtMs)));
  return input.update(levels, changedAtMs + 30U);
}

void initializeReleased(ThreeKeyInput &input, uint32_t startedAtMs = 0) {
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(ThreeKeyEvent::None),
      static_cast<int>(input.update(released(), startedAtMs)));
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(ThreeKeyEvent::None),
      static_cast<int>(input.update(released(), startedAtMs + 30U)));
}

void apply(CompanionController &controller, ThreeKeyEvent event, uint32_t nowMs) {
  switch (event) {
    case ThreeKeyEvent::LeftShort:
      controller.onPreviousPage();
      break;
    case ThreeKeyEvent::CenterShort:
      controller.onCenterShort(nowMs);
      break;
    case ThreeKeyEvent::CenterLong:
      controller.onCenterLong();
      break;
    case ThreeKeyEvent::RightShort:
      controller.onNextPage();
      break;
    case ThreeKeyEvent::None:
      break;
  }
}
}  // namespace

static void test_a1_left_and_right_cover_all_ring_transitions() {
  CompanionController controller;

  controller.onPreviousPage();
  TEST_ASSERT_EQUAL_INT(static_cast<int>(Page::Focus), static_cast<int>(controller.page()));
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(ActionFeedback::PageFocus), static_cast<int>(controller.actionFeedback()));
  controller.onPreviousPage();
  TEST_ASSERT_EQUAL_INT(static_cast<int>(Page::Activity), static_cast<int>(controller.page()));
  controller.onPreviousPage();
  TEST_ASSERT_EQUAL_INT(static_cast<int>(Page::Overview), static_cast<int>(controller.page()));

  controller.onNextPage();
  TEST_ASSERT_EQUAL_INT(static_cast<int>(Page::Activity), static_cast<int>(controller.page()));
  controller.onNextPage();
  TEST_ASSERT_EQUAL_INT(static_cast<int>(Page::Focus), static_cast<int>(controller.page()));
  controller.onNextPage();
  TEST_ASSERT_EQUAL_INT(static_cast<int>(Page::Overview), static_cast<int>(controller.page()));
}

static void test_a2_center_short_starts_pauses_and_resumes_focus() {
  CompanionController controller;

  controller.onCenterShort(1000);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(Page::Focus), static_cast<int>(controller.page()));
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(FocusRunState::Running), static_cast<int>(controller.focusRunState()));
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(ActionFeedback::FocusStarted), static_cast<int>(controller.actionFeedback()));
  controller.tick(61000);
  TEST_ASSERT_EQUAL_UINT32(24U * 60U, controller.remainingSeconds());

  controller.onCenterShort(61000);
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(FocusRunState::Paused), static_cast<int>(controller.focusRunState()));
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(ActionFeedback::FocusPaused), static_cast<int>(controller.actionFeedback()));
  controller.onPreviousPage();
  controller.onCenterShort(121000);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(Page::Focus), static_cast<int>(controller.page()));
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(FocusRunState::Running), static_cast<int>(controller.focusRunState()));
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(ActionFeedback::FocusResumed), static_cast<int>(controller.actionFeedback()));
  controller.tick(181000);
  TEST_ASSERT_EQUAL_UINT32(23U * 60U, controller.remainingSeconds());
}

static void test_a3_center_long_is_once_and_suppresses_short_release() {
  ThreeKeyInput input(30, 800);
  CompanionController controller;
  initializeReleased(input);
  controller.onCenterShort(0);
  controller.tick(60000);

  const ThreeKeyLevels centerDown = {false, true, false};
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(ThreeKeyEvent::None), static_cast<int>(updateTwice(input, centerDown, 100)));
  const ThreeKeyEvent longEvent = input.update(centerDown, 930);
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(ThreeKeyEvent::CenterLong), static_cast<int>(longEvent));
  apply(controller, longEvent, 930);
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(FocusRunState::Ready), static_cast<int>(controller.focusRunState()));
  TEST_ASSERT_EQUAL_UINT32(25U * 60U, controller.remainingSeconds());
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(ActionFeedback::FocusReset), static_cast<int>(controller.actionFeedback()));
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(ThreeKeyEvent::None), static_cast<int>(input.update(centerDown, 1500)));
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(ThreeKeyEvent::None),
      static_cast<int>(updateTwice(input, released(), 1600)));
}

static void test_a4_center_long_outside_focus_only_requests_voice_off() {
  CompanionController controller;

  const uint32_t initialRemaining = controller.remainingSeconds();
  TEST_ASSERT_TRUE(controller.onCenterLong());
  TEST_ASSERT_EQUAL_INT(static_cast<int>(Page::Overview), static_cast<int>(controller.page()));
  TEST_ASSERT_EQUAL_UINT32(initialRemaining, controller.remainingSeconds());
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(ActionFeedback::VoiceOff), static_cast<int>(controller.actionFeedback()));

  controller.onNextPage();
  TEST_ASSERT_TRUE(controller.onCenterLong());
  TEST_ASSERT_EQUAL_INT(static_cast<int>(Page::Activity), static_cast<int>(controller.page()));
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(FocusRunState::Ready), static_cast<int>(controller.focusRunState()));
  TEST_ASSERT_EQUAL_UINT32(initialRemaining, controller.remainingSeconds());
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(ActionFeedback::VoiceOff), static_cast<int>(controller.actionFeedback()));
}

static void test_a5_each_short_emits_on_debounced_release_without_double_wait() {
  const ThreeKeyLevels pressedLevels[] = {
      {true, false, false}, {false, true, false}, {false, false, true}};
  const ThreeKeyEvent expected[] = {
      ThreeKeyEvent::LeftShort, ThreeKeyEvent::CenterShort, ThreeKeyEvent::RightShort};

  for (int index = 0; index < 3; ++index) {
    ThreeKeyInput input(30, 800);
    initializeReleased(input);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(ThreeKeyEvent::None),
        static_cast<int>(updateTwice(input, pressedLevels[index], 100)));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(ThreeKeyEvent::None),
        static_cast<int>(input.update(released(), 200)));
    const ThreeKeyEvent event = input.update(released(), 230);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(expected[index]), static_cast<int>(event));
    TEST_ASSERT_TRUE(230U - 200U <= 100U);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(ThreeKeyEvent::None), static_cast<int>(input.update(released(), 580)));
  }
}

static void test_a6_bounce_and_sustained_press_do_not_repeat() {
  ThreeKeyInput input(30, 800);
  initializeReleased(input);
  const ThreeKeyLevels leftDown = {true, false, false};

  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(ThreeKeyEvent::None), static_cast<int>(input.update(leftDown, 40)));
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(ThreeKeyEvent::None), static_cast<int>(input.update(released(), 50)));
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(ThreeKeyEvent::None), static_cast<int>(input.update(leftDown, 60)));
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(ThreeKeyEvent::None), static_cast<int>(input.update(leftDown, 90)));
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(ThreeKeyEvent::None), static_cast<int>(input.update(leftDown, 1000)));
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(ThreeKeyEvent::None), static_cast<int>(input.update(released(), 1100)));
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(ThreeKeyEvent::LeftShort), static_cast<int>(input.update(released(), 1130)));
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(ThreeKeyEvent::None), static_cast<int>(input.update(released(), 1500)));
}

static void test_a7_simultaneous_keys_choose_one_owner_by_priority() {
  ThreeKeyInput input(30, 800);
  initializeReleased(input);
  const ThreeKeyLevels allDown = {true, true, true};
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(ThreeKeyEvent::None), static_cast<int>(updateTwice(input, allDown, 100)));
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(ThreeKeyEvent::None), static_cast<int>(input.update(allDown, 500)));

  const ThreeKeyLevels centerAndRightDown = {false, true, true};
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(ThreeKeyEvent::None),
      static_cast<int>(input.update(centerAndRightDown, 600)));
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(ThreeKeyEvent::LeftShort),
      static_cast<int>(input.update(centerAndRightDown, 630)));
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(ThreeKeyEvent::None),
      static_cast<int>(updateTwice(input, released(), 700)));

  const ThreeKeyLevels centerDown = {false, true, false};
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(ThreeKeyEvent::None), static_cast<int>(updateTwice(input, centerDown, 800)));
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(ThreeKeyEvent::CenterShort),
      static_cast<int>(updateTwice(input, released(), 900)));
}

static void test_a8_debounce_and_center_long_survive_millis_wraparound() {
  ThreeKeyInput input(30, 800);
  const uint32_t initialAt = UINT32_MAX - 50U;
  initializeReleased(input, initialAt);
  const ThreeKeyLevels centerDown = {false, true, false};

  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(ThreeKeyEvent::None),
      static_cast<int>(input.update(centerDown, UINT32_MAX - 10U)));
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(ThreeKeyEvent::None), static_cast<int>(input.update(centerDown, 20U)));
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(ThreeKeyEvent::CenterLong), static_cast<int>(input.update(centerDown, 820U)));
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(ThreeKeyEvent::None), static_cast<int>(input.update(centerDown, 1820U)));
}

static void test_a9_navigation_voice_off_and_missing_host_do_not_mutate_timer() {
  CompanionController controller;
  controller.onCenterShort(0);
  controller.tick(1000);
  TEST_ASSERT_EQUAL_UINT32(1499U, controller.remainingSeconds());

  controller.onPreviousPage();
  TEST_ASSERT_EQUAL_UINT32(1499U, controller.remainingSeconds());
  TEST_ASSERT_TRUE(controller.onCenterLong());
  TEST_ASSERT_EQUAL_UINT32(1499U, controller.remainingSeconds());
  controller.tick(2000);
  TEST_ASSERT_EQUAL_UINT32(1498U, controller.remainingSeconds());

  controller.onCenterShort(2000);
  controller.tick(12000);
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(FocusRunState::Paused), static_cast<int>(controller.focusRunState()));
  TEST_ASSERT_EQUAL_UINT32(1498U, controller.remainingSeconds());
}

static void test_a10_profiles_keep_three_key_and_legacy_contracts_isolated() {
  TEST_ASSERT_FALSE(BoardInputConfig::kThreeKeyEnabled);

  ThreeKeyInput threeKey(30, 800);
  initializeReleased(threeKey);
  const ThreeKeyLevels rightDown = {false, false, true};
  updateTwice(threeKey, rightDown, 100);
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(ThreeKeyEvent::RightShort),
      static_cast<int>(updateTwice(threeKey, released(), 200)));

  KeyGestureDetector legacy(30, 350, 800);
  legacy.update(true, 0);
  legacy.update(true, 30);
  legacy.update(false, 80);
  legacy.update(false, 110);
  legacy.update(true, 200);
  legacy.update(true, 230);
  legacy.update(false, 280);
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(KeyGesture::Double), static_cast<int>(legacy.update(false, 310)));
}

int main(int argc, char **argv) {
  UNITY_BEGIN();
  RUN_TEST(test_a1_left_and_right_cover_all_ring_transitions);
  RUN_TEST(test_a2_center_short_starts_pauses_and_resumes_focus);
  RUN_TEST(test_a3_center_long_is_once_and_suppresses_short_release);
  RUN_TEST(test_a4_center_long_outside_focus_only_requests_voice_off);
  RUN_TEST(test_a5_each_short_emits_on_debounced_release_without_double_wait);
  RUN_TEST(test_a6_bounce_and_sustained_press_do_not_repeat);
  RUN_TEST(test_a7_simultaneous_keys_choose_one_owner_by_priority);
  RUN_TEST(test_a8_debounce_and_center_long_survive_millis_wraparound);
  RUN_TEST(test_a9_navigation_voice_off_and_missing_host_do_not_mutate_timer);
  RUN_TEST(test_a10_profiles_keep_three_key_and_legacy_contracts_isolated);
  return UNITY_END();
}
