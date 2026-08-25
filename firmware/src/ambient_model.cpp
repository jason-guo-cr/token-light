#include "ambient_model.h"

#include <math.h>

AmbientModel::AmbientModel(uint32_t staleAfterMs)
    : staleAfterMs_(staleAfterMs),
      validAtMs_(0),
      temperatureC_(0),
      humidityPercent_(0),
      hasValidSample_(false),
      lastReadFailed_(false) {}

bool AmbientModel::acceptSample(float temperatureC, float humidityPercent, uint32_t nowMs) {
  if (!isfinite(temperatureC) || !isfinite(humidityPercent) || temperatureC < -40.0f ||
      temperatureC > 125.0f || humidityPercent < 0.0f || humidityPercent > 100.0f) {
    markReadFailed(nowMs);
    return false;
  }

  temperatureC_ = static_cast<int>(lroundf(temperatureC));
  humidityPercent_ = static_cast<int>(lroundf(humidityPercent));
  validAtMs_ = nowMs;
  hasValidSample_ = true;
  lastReadFailed_ = false;
  return true;
}

void AmbientModel::markReadFailed(uint32_t nowMs) {
  (void)nowMs;
  if (hasValidSample_) {
    lastReadFailed_ = true;
  }
}

AmbientSnapshot AmbientModel::snapshot(uint32_t nowMs) const {
  const bool fresh = hasValidSample_ && nowMs - validAtMs_ <= staleAfterMs_;
  return {
      fresh,
      fresh && lastReadFailed_,
      fresh ? temperatureC_ : 0,
      fresh ? humidityPercent_ : 0,
  };
}
