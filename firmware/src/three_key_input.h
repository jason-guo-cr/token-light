#pragma once

#include <stdint.h>

enum class ThreeKeyEvent {
  None,
  LeftShort,
  CenterShort,
  CenterLong,
  RightShort,
};

struct ThreeKeyLevels {
  bool leftPressed;
  bool centerPressed;
  bool rightPressed;
};

class ThreeKeyInput {
 public:
  ThreeKeyInput(uint32_t debounceMs, uint32_t centerLongPressMs);

  ThreeKeyEvent update(const ThreeKeyLevels &levels, uint32_t nowMs);

 private:
  enum class Key {
    None,
    Left,
    Center,
    Right,
  };

  enum class Edge {
    None,
    Pressed,
    Released,
  };

  struct DebouncedKey {
    bool sampled;
    bool settled;
    bool rawPressed;
    bool stablePressed;
    uint32_t rawChangedAtMs;
  };

  Edge updateKey(DebouncedKey &key, bool pressed, uint32_t nowMs);
  bool allReleased() const;
  bool isStablePressed(Key key) const;
  ThreeKeyEvent shortEvent(Key key) const;

  uint32_t debounceMs_;
  uint32_t centerLongPressMs_;
  DebouncedKey left_;
  DebouncedKey center_;
  DebouncedKey right_;
  Key owner_;
  bool ownerStartedDuringBoot_;
  bool centerLongEmitted_;
  bool waitingForAllReleased_;
  uint32_t ownerPressedAtMs_;
};
