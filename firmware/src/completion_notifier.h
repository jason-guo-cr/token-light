#pragma once

#include <stdint.h>

class CompletionNotifier {
 public:
  CompletionNotifier();

  bool observe(uint32_t completionSeq, uint32_t nowMs);
  bool shouldPlay(int localHour, bool enabled, bool audioAlways) const;
  bool celebrating(uint32_t nowMs) const;
  bool hasCelebration() const;
  uint32_t celebrationUntilMs() const;

 private:
  bool initialized_;
  uint32_t lastSequence_;
  bool celebrationActive_;
  uint32_t celebrationUntilMs_;
};
