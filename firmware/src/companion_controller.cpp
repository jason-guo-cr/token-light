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
      intervalCompleted_(false),
      actionFeedback_(ActionFeedback::None) {}

Page CompanionController::page() const { return page_; }

FocusPhase CompanionController::focusPhase() const { return focusPhase_; }

FocusRunState CompanionController::focusRunState() const { return focusRunState_; }

uint32_t CompanionController::remainingSeconds() const { return remainingSeconds_; }

ActionFeedback CompanionController::actionFeedback() const { return actionFeedback_; }

void CompanionController::setPageFeedback() {
  switch (page_) {
    case Page::Overview:
      actionFeedback_ = ActionFeedback::PageOverview;
      break;
    case Page::Activity:
      actionFeedback_ = ActionFeedback::PageActivity;
      break;
    case Page::Focus:
      actionFeedback_ = ActionFeedback::PageFocus;
      break;
  }
}

void CompanionController::onPreviousPage() {
  switch (page_) {
    case Page::Overview:
      page_ = Page::Focus;
      break;
    case Page::Activity:
      page_ = Page::Overview;
      break;
    case Page::Focus:
      page_ = Page::Activity;
      break;
  }
  setPageFeedback();
}

void CompanionController::onNextPage() {
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
  setPageFeedback();
}

void CompanionController::onCenterShort(uint32_t nowMs) {
  page_ = Page::Focus;
  if (focusRunState_ == FocusRunState::Running) {
    tick(nowMs);
    if (focusRunState_ == FocusRunState::Running) {
      focusRunState_ = FocusRunState::Paused;
      actionFeedback_ = ActionFeedback::FocusPaused;
    }
    return;
  }

  const bool resuming = focusRunState_ == FocusRunState::Paused;
  focusRunState_ = FocusRunState::Running;
  lastTickMs_ = nowMs;
  actionFeedback_ = resuming ? ActionFeedback::FocusResumed : ActionFeedback::FocusStarted;
}

bool CompanionController::onCenterLong() {
  if (page_ != Page::Focus) {
    actionFeedback_ = ActionFeedback::VoiceOff;
    return true;
  }
  resetFocus();
  actionFeedback_ = ActionFeedback::FocusReset;
  return false;
}

void CompanionController::onShortPress() { onNextPage(); }

void CompanionController::onDoublePress(uint32_t nowMs) { onCenterShort(nowMs); }

bool CompanionController::onLongPress() { return onCenterLong(); }

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
