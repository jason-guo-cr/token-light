#pragma once

#include <stdint.h>

enum class KeyGesture {
  None,
  Short,
  Double,
  Long,
};

class KeyGestureDetector {
 public:
  KeyGestureDetector(uint32_t debounceMs, uint32_t doubleClickMs, uint32_t longPressMs);

  KeyGesture update(bool pressed, uint32_t nowMs);

 private:
  uint32_t debounceMs_;
  uint32_t doubleClickMs_;
  uint32_t longPressMs_;
  bool rawPressed_;
  bool stablePressed_;
  bool pendingShort_;
  bool secondPressCandidate_;
  bool longEmitted_;
  uint32_t rawChangedAtMs_;
  uint32_t pressedAtMs_;
  uint32_t firstReleasedAtMs_;
};
