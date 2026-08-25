#include "completion_notifier.h"

CompletionNotifier::CompletionNotifier() : initialized_(false), lastSequence_(0) {}

bool CompletionNotifier::shouldPlay(
    uint32_t completionSeq, int localHour, bool enabled, bool audioAlways) {
  if (!initialized_) {
    initialized_ = true;
    lastSequence_ = completionSeq;
    return false;
  }
  if (completionSeq <= lastSequence_) {
    return false;
  }

  lastSequence_ = completionSeq;
  const bool quietHours = localHour >= 22 || localHour < 8;
  return enabled && (audioAlways || !quietHours);
}
