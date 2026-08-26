#include <unity.h>

#include "page_footer_labels.h"

static void test_three_key_focus_footer_preserves_navigation_and_adds_reset_hint() {
  const PageFooterLabels footer = focusFooterLabels(true);

  TEST_ASSERT_EQUAL_STRING("LEFT: PREV  CENTER: FOCUS  RIGHT: NEXT", footer.primary);
  TEST_ASSERT_EQUAL_STRING("HOLD CENTER: RESET", footer.secondary);
}

static void test_legacy_focus_footer_remains_unchanged() {
  const PageFooterLabels footer = focusFooterLabels(false);

  TEST_ASSERT_EQUAL_STRING("DOUBLE: START / PAUSE   HOLD: RESET", footer.primary);
  TEST_ASSERT_NULL(footer.secondary);
}

int main(int argc, char **argv) {
  UNITY_BEGIN();
  RUN_TEST(test_three_key_focus_footer_preserves_navigation_and_adds_reset_hint);
  RUN_TEST(test_legacy_focus_footer_remains_unchanged);
  return UNITY_END();
}
