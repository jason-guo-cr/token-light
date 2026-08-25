#pragma once

#include <stdint.h>

class CompletionNotifier {
 public:
  CompletionNotifier();

  bool shouldPlay(uint32_t completionSeq, int localHour, bool enabled, bool audioAlways);

 private:
  bool initialized_;
  uint32_t lastSequence_;
};
