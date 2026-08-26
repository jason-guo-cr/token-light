#pragma once

#include <stdint.h>

enum class Page {
  Overview,
  Activity,
  Focus,
};

enum class FocusPhase {
  Focus,
  Break,
};

enum class FocusRunState {
  Ready,
  Running,
  Paused,
};

enum class ActionFeedback {
  None,
  PageOverview,
  PageActivity,
  PageFocus,
  FocusStarted,
  FocusPaused,
  FocusResumed,
  FocusReset,
  VoiceOff,
};

class CompanionController {
 public:
  CompanionController();

  Page page() const;
  FocusPhase focusPhase() const;
  FocusRunState focusRunState() const;
  uint32_t remainingSeconds() const;
  ActionFeedback actionFeedback() const;

  void onPreviousPage();
  void onNextPage();
  void onCenterShort(uint32_t nowMs);
  bool onCenterLong();
  void onShortPress();
  void onDoublePress(uint32_t nowMs);
  bool onLongPress();
  void tick(uint32_t nowMs);
  bool consumeIntervalCompleted();

 private:
  void setPageFeedback();
  void resetFocus();

  Page page_;
  FocusPhase focusPhase_;
  FocusRunState focusRunState_;
  uint32_t remainingSeconds_;
  uint32_t lastTickMs_;
  uint32_t partialSecondMs_;
  bool intervalCompleted_;
  ActionFeedback actionFeedback_;
};
