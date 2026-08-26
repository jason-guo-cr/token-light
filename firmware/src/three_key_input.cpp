#include "three_key_input.h"

ThreeKeyInput::ThreeKeyInput(uint32_t debounceMs, uint32_t centerLongPressMs)
    : debounceMs_(debounceMs),
      centerLongPressMs_(centerLongPressMs),
      left_{false, false, false, false, 0},
      center_{false, false, false, false, 0},
      right_{false, false, false, false, 0},
      owner_(Key::None),
      ownerStartedDuringBoot_(false),
      centerLongEmitted_(false),
      waitingForAllReleased_(false),
      ownerPressedAtMs_(0) {}

ThreeKeyInput::Edge ThreeKeyInput::updateKey(
    DebouncedKey &key, bool pressed, uint32_t nowMs) {
  if (!key.sampled) {
    key.sampled = true;
    key.rawPressed = pressed;
    key.rawChangedAtMs = nowMs;
    return Edge::None;
  }

  if (pressed != key.rawPressed) {
    key.rawPressed = pressed;
    key.rawChangedAtMs = nowMs;
  }

  if (nowMs - key.rawChangedAtMs < debounceMs_) {
    return Edge::None;
  }

  if (!key.settled) {
    key.settled = true;
    key.stablePressed = key.rawPressed;
    return key.stablePressed ? Edge::Pressed : Edge::None;
  }

  if (key.stablePressed == key.rawPressed) {
    return Edge::None;
  }

  key.stablePressed = key.rawPressed;
  return key.stablePressed ? Edge::Pressed : Edge::Released;
}

bool ThreeKeyInput::allReleased() const {
  return left_.settled && center_.settled && right_.settled && !left_.rawPressed &&
         !center_.rawPressed && !right_.rawPressed && !left_.stablePressed &&
         !center_.stablePressed && !right_.stablePressed;
}

bool ThreeKeyInput::isStablePressed(Key key) const {
  switch (key) {
    case Key::Left:
      return left_.stablePressed;
    case Key::Center:
      return center_.stablePressed;
    case Key::Right:
      return right_.stablePressed;
    case Key::None:
      return false;
  }
  return false;
}

ThreeKeyEvent ThreeKeyInput::shortEvent(Key key) const {
  switch (key) {
    case Key::Left:
      return ThreeKeyEvent::LeftShort;
    case Key::Center:
      return ThreeKeyEvent::CenterShort;
    case Key::Right:
      return ThreeKeyEvent::RightShort;
    case Key::None:
      return ThreeKeyEvent::None;
  }
  return ThreeKeyEvent::None;
}

ThreeKeyEvent ThreeKeyInput::update(const ThreeKeyLevels &levels, uint32_t nowMs) {
  const bool leftWasSettled = left_.settled;
  const bool centerWasSettled = center_.settled;
  const bool rightWasSettled = right_.settled;
  const Edge leftEdge = updateKey(left_, levels.leftPressed, nowMs);
  const Edge centerEdge = updateKey(center_, levels.centerPressed, nowMs);
  const Edge rightEdge = updateKey(right_, levels.rightPressed, nowMs);

  if (waitingForAllReleased_) {
    if (allReleased()) {
      waitingForAllReleased_ = false;
    }
    return ThreeKeyEvent::None;
  }

  if (owner_ == Key::None) {
    Key selected = Key::None;
    bool startedDuringBoot = false;
    if (leftEdge == Edge::Pressed) {
      selected = Key::Left;
      startedDuringBoot = !leftWasSettled;
    } else if (centerEdge == Edge::Pressed) {
      selected = Key::Center;
      startedDuringBoot = !centerWasSettled;
    } else if (rightEdge == Edge::Pressed) {
      selected = Key::Right;
      startedDuringBoot = !rightWasSettled;
    }

    if (selected != Key::None) {
      owner_ = selected;
      ownerStartedDuringBoot_ = startedDuringBoot;
      centerLongEmitted_ = false;
      ownerPressedAtMs_ = nowMs;
    }
  }

  if (owner_ == Key::Center && isStablePressed(owner_) && !centerLongEmitted_ &&
      nowMs - ownerPressedAtMs_ >= centerLongPressMs_) {
    centerLongEmitted_ = true;
    return ThreeKeyEvent::CenterLong;
  }

  const bool ownerReleased =
      (owner_ == Key::Left && leftEdge == Edge::Released) ||
      (owner_ == Key::Center && centerEdge == Edge::Released) ||
      (owner_ == Key::Right && rightEdge == Edge::Released);
  if (!ownerReleased) {
    return ThreeKeyEvent::None;
  }

  const Key releasedOwner = owner_;
  const bool emitShort = !ownerStartedDuringBoot_ && !centerLongEmitted_;
  owner_ = Key::None;
  ownerStartedDuringBoot_ = false;
  centerLongEmitted_ = false;
  waitingForAllReleased_ = !allReleased();
  return emitShort ? shortEvent(releasedOwner) : ThreeKeyEvent::None;
}
