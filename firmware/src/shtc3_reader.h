#pragma once

#include <Arduino.h>
#include <Wire.h>

#include "ambient_model.h"

class Shtc3Reader {
 public:
  explicit Shtc3Reader(TwoWire &wire);

  void begin(uint32_t nowMs);
  void tick(AmbientModel &model, uint32_t nowMs);

 private:
  bool requestMeasurement(uint32_t nowMs);
  bool readMeasurement(float &temperatureC, float &humidityPercent);
  static uint8_t crc8(const uint8_t *data, size_t length);

  TwoWire &wire_;
  bool measuring_;
  uint32_t requestedAtMs_;
  uint32_t nextSampleAtMs_;
};
