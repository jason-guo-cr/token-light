#include "completion_notifier.h"

CompletionNotifier::CompletionNotifier()
    : initialized_(false), lastSequence_(0), celebrationActive_(false), celebrationUntilMs_(0) {}

bool CompletionNotifier::observe(uint32_t completionSeq, uint32_t nowMs) {
  if (!initialized_) {
    initialized_ = true;
    lastSequence_ = completionSeq;
    return false;
  }
  if (completionSeq == lastSequence_) {
    return false;
  }
  if (completionSeq < lastSequence_) {
    lastSequence_ = completionSeq;
    return false;
  }

  lastSequence_ = completionSeq;
  celebrationActive_ = true;
  celebrationUntilMs_ = nowMs + 30000U;
  return true;
}

bool CompletionNotifier::shouldPlay(int localHour, bool enabled, bool audioAlways) const {
  const bool quietHours = localHour >= 22 || localHour < 8;
  return enabled && (audioAlways || !quietHours);
}

bool CompletionNotifier::celebrating(uint32_t nowMs) const {
  return celebrationActive_ && static_cast<int32_t>(celebrationUntilMs_ - nowMs) > 0;
}

bool CompletionNotifier::hasCelebration() const {
  return celebrationActive_;
}

uint32_t CompletionNotifier::celebrationUntilMs() const {
  return celebrationUntilMs_;
}
