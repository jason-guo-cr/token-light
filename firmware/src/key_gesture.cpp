#include "key_gesture.h"

KeyGestureDetector::KeyGestureDetector(
    uint32_t debounceMs, uint32_t doubleClickMs, uint32_t longPressMs)
    : debounceMs_(debounceMs),
      doubleClickMs_(doubleClickMs),
      longPressMs_(longPressMs),
      rawPressed_(false),
      stablePressed_(false),
      pendingShort_(false),
      secondPressCandidate_(false),
      longEmitted_(false),
      rawChangedAtMs_(0),
      pressedAtMs_(0),
      firstReleasedAtMs_(0) {}

KeyGesture KeyGestureDetector::update(bool pressed, uint32_t nowMs) {
  if (pressed != rawPressed_) {
    rawPressed_ = pressed;
    rawChangedAtMs_ = nowMs;
  }

  if (rawPressed_ != stablePressed_ && nowMs - rawChangedAtMs_ >= debounceMs_) {
    stablePressed_ = rawPressed_;
    if (stablePressed_) {
      pressedAtMs_ = nowMs;
      longEmitted_ = false;
      secondPressCandidate_ =
          pendingShort_ && nowMs - firstReleasedAtMs_ <= doubleClickMs_;
    } else if (longEmitted_) {
      secondPressCandidate_ = false;
    } else if (secondPressCandidate_) {
      pendingShort_ = false;
      secondPressCandidate_ = false;
      return KeyGesture::Double;
    } else {
      pendingShort_ = true;
      firstReleasedAtMs_ = nowMs;
    }
  }

  if (stablePressed_ && !longEmitted_ && nowMs - pressedAtMs_ >= longPressMs_) {
    longEmitted_ = true;
    pendingShort_ = false;
    secondPressCandidate_ = false;
    return KeyGesture::Long;
  }

  if (pendingShort_ && !stablePressed_ && nowMs - firstReleasedAtMs_ > doubleClickMs_) {
    pendingShort_ = false;
    return KeyGesture::Short;
  }

  return KeyGesture::None;
}
