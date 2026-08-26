#include "companion_controller.h"

namespace {
constexpr uint32_t kFocusSeconds = 25U * 60U;
constexpr uint32_t kBreakSeconds = 5U * 60U;
}

CompanionController::CompanionController()
    : page_(Page::Overview),
      focusPhase_(FocusPhase::Focus),
      focusRunState_(FocusRunState::Ready),
      remainingSeconds_(kFocusSeconds),
      lastTickMs_(0),
      partialSecondMs_(0),
      intervalCompleted_(false) {}

Page CompanionController::page() const { return page_; }

FocusPhase CompanionController::focusPhase() const { return focusPhase_; }

FocusRunState CompanionController::focusRunState() const { return focusRunState_; }

uint32_t CompanionController::remainingSeconds() const { return remainingSeconds_; }

void CompanionController::onShortPress() {
  switch (page_) {
    case Page::Overview:
      page_ = Page::Activity;
      break;
    case Page::Activity:
      page_ = Page::Focus;
      break;
    case Page::Focus:
      page_ = Page::Overview;
      break;
  }
}

void CompanionController::onDoublePress(uint32_t nowMs) {
  page_ = Page::Focus;
  if (focusRunState_ == FocusRunState::Running) {
    tick(nowMs);
    if (focusRunState_ == FocusRunState::Running) {
      focusRunState_ = FocusRunState::Paused;
    }
    return;
  }

  focusRunState_ = FocusRunState::Running;
  lastTickMs_ = nowMs;
}

bool CompanionController::onLongPress() {
  if (page_ != Page::Focus) {
    return true;
  }
  resetFocus();
  return false;
}

void CompanionController::tick(uint32_t nowMs) {
  if (focusRunState_ != FocusRunState::Running) {
    return;
  }

  const uint32_t elapsedMs = nowMs - lastTickMs_;
  lastTickMs_ = nowMs;
  const uint32_t totalMs = partialSecondMs_ + elapsedMs;
  const uint32_t elapsedSeconds = totalMs / 1000U;
  partialSecondMs_ = totalMs % 1000U;
  if (elapsedSeconds == 0) {
    return;
  }

  if (elapsedSeconds < remainingSeconds_) {
    remainingSeconds_ -= elapsedSeconds;
    return;
  }

  focusPhase_ = focusPhase_ == FocusPhase::Focus ? FocusPhase::Break : FocusPhase::Focus;
  remainingSeconds_ = focusPhase_ == FocusPhase::Focus ? kFocusSeconds : kBreakSeconds;
  focusRunState_ = FocusRunState::Ready;
  partialSecondMs_ = 0;
  intervalCompleted_ = true;
}

bool CompanionController::consumeIntervalCompleted() {
  const bool completed = intervalCompleted_;
  intervalCompleted_ = false;
  return completed;
}

void CompanionController::resetFocus() {
  page_ = Page::Focus;
  focusPhase_ = FocusPhase::Focus;
  focusRunState_ = FocusRunState::Ready;
  remainingSeconds_ = kFocusSeconds;
  partialSecondMs_ = 0;
  intervalCompleted_ = false;
}
