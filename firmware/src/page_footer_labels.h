#pragma once

struct PageFooterLabels {
  const char *primary;
  const char *secondary;
};

inline PageFooterLabels navigationFooterLabels(bool threeKeyProfile) {
  return threeKeyProfile
             ? PageFooterLabels{"LEFT: PREV  CENTER: FOCUS  RIGHT: NEXT", nullptr}
             : PageFooterLabels{"SHORT: NEXT   DOUBLE: FOCUS   HOLD: VOICE", nullptr};
}

inline PageFooterLabels focusFooterLabels(bool threeKeyProfile) {
  return threeKeyProfile
             ? PageFooterLabels{
                   "LEFT: PREV  CENTER: FOCUS  RIGHT: NEXT", "HOLD CENTER: RESET"}
             : PageFooterLabels{"DOUBLE: START / PAUSE   HOLD: RESET", nullptr};
}
