#include <stdint.h>
#include <unity.h>

#include "key_gesture.h"

static void test_bounce_produces_one_short_press_after_double_window() {
  KeyGestureDetector detector(30, 350, 800);

  TEST_ASSERT_EQUAL_INT(static_cast<int>(KeyGesture::None), static_cast<int>(detector.update(true, 10)));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(KeyGesture::None), static_cast<int>(detector.update(false, 20)));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(KeyGesture::None), static_cast<int>(detector.update(true, 40)));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(KeyGesture::None), static_cast<int>(detector.update(true, 70)));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(KeyGesture::None), static_cast<int>(detector.update(false, 120)));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(KeyGesture::None), static_cast<int>(detector.update(false, 150)));

  TEST_ASSERT_EQUAL_INT(static_cast<int>(KeyGesture::Short), static_cast<int>(detector.update(false, 501)));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(KeyGesture::None), static_cast<int>(detector.update(false, 600)));
}

static void test_double_press_does_not_emit_short_press() {
  KeyGestureDetector detector(30, 350, 800);
  detector.update(true, 0);
  detector.update(true, 30);
  detector.update(false, 80);
  detector.update(false, 110);
  detector.update(true, 200);
  detector.update(true, 230);
  detector.update(false, 280);

  TEST_ASSERT_EQUAL_INT(static_cast<int>(KeyGesture::Double), static_cast<int>(detector.update(false, 310)));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(KeyGesture::None), static_cast<int>(detector.update(false, 700)));
}

static void test_long_press_emits_once_and_never_emits_short() {
  KeyGestureDetector detector(30, 350, 800);
  detector.update(true, 0);
  detector.update(true, 30);

  TEST_ASSERT_EQUAL_INT(static_cast<int>(KeyGesture::Long), static_cast<int>(detector.update(true, 830)));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(KeyGesture::None), static_cast<int>(detector.update(true, 1000)));
  detector.update(false, 1100);
  detector.update(false, 1130);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(KeyGesture::None), static_cast<int>(detector.update(false, 1600)));
}

int main(int argc, char **argv) {
  UNITY_BEGIN();
  RUN_TEST(test_bounce_produces_one_short_press_after_double_window);
  RUN_TEST(test_double_press_does_not_emit_short_press);
  RUN_TEST(test_long_press_emits_once_and_never_emits_short);
  return UNITY_END();
}
