#pragma once

#include <stdint.h>

struct AmbientSnapshot {
  bool valid;
  bool cached;
  int temperatureC;
  int humidityPercent;
};

class AmbientModel {
 public:
  explicit AmbientModel(uint32_t staleAfterMs);

  bool acceptSample(float temperatureC, float humidityPercent, uint32_t nowMs);
  void markReadFailed(uint32_t nowMs);
  AmbientSnapshot snapshot(uint32_t nowMs) const;

 private:
  uint32_t staleAfterMs_;
  uint32_t validAtMs_;
  int temperatureC_;
  int humidityPercent_;
  bool hasValidSample_;
  bool lastReadFailed_;
};
