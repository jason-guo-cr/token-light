#include "shtc3_reader.h"

namespace {
constexpr uint8_t kAddress = 0x70;
constexpr uint32_t kMeasurementWaitMs = 20;
constexpr uint32_t kSampleIntervalMs = 60000;
}

Shtc3Reader::Shtc3Reader(TwoWire &wire)
    : wire_(wire), measuring_(false), requestedAtMs_(0), nextSampleAtMs_(0) {}

void Shtc3Reader::begin(uint32_t nowMs) {
  nextSampleAtMs_ = nowMs;
}

void Shtc3Reader::tick(AmbientModel &model, uint32_t nowMs) {
  if (measuring_) {
    if (nowMs - requestedAtMs_ < kMeasurementWaitMs) {
      return;
    }
    float temperatureC = 0;
    float humidityPercent = 0;
    if (readMeasurement(temperatureC, humidityPercent)) {
      model.acceptSample(temperatureC, humidityPercent, nowMs);
    } else {
      model.markReadFailed(nowMs);
    }
    measuring_ = false;
    nextSampleAtMs_ = nowMs + kSampleIntervalMs;
    return;
  }

  if (static_cast<int32_t>(nowMs - nextSampleAtMs_) >= 0 && !requestMeasurement(nowMs)) {
    model.markReadFailed(nowMs);
    nextSampleAtMs_ = nowMs + kSampleIntervalMs;
  }
}

bool Shtc3Reader::requestMeasurement(uint32_t nowMs) {
  wire_.beginTransmission(kAddress);
  wire_.write(0x78);
  wire_.write(0x66);
  if (wire_.endTransmission() != 0) {
    return false;
  }
  requestedAtMs_ = nowMs;
  measuring_ = true;
  return true;
}

bool Shtc3Reader::readMeasurement(float &temperatureC, float &humidityPercent) {
  if (wire_.requestFrom(kAddress, static_cast<uint8_t>(6)) != 6) {
    while (wire_.available()) {
      wire_.read();
    }
    return false;
  }
  uint8_t bytes[6];
  for (uint8_t &byte : bytes) {
    byte = static_cast<uint8_t>(wire_.read());
  }
  if (crc8(bytes, 2) != bytes[2] || crc8(bytes + 3, 2) != bytes[5]) {
    return false;
  }
  const uint16_t rawTemperature = static_cast<uint16_t>(bytes[0] << 8) | bytes[1];
  const uint16_t rawHumidity = static_cast<uint16_t>(bytes[3] << 8) | bytes[4];
  temperatureC = -45.0f + 175.0f * static_cast<float>(rawTemperature) / 65535.0f;
  humidityPercent = 100.0f * static_cast<float>(rawHumidity) / 65535.0f;
  return true;
}

uint8_t Shtc3Reader::crc8(const uint8_t *data, size_t length) {
  uint8_t crc = 0xFF;
  for (size_t index = 0; index < length; ++index) {
    crc ^= data[index];
    for (uint8_t bit = 0; bit < 8; ++bit) {
      crc = (crc & 0x80) ? static_cast<uint8_t>((crc << 1) ^ 0x31) : static_cast<uint8_t>(crc << 1);
    }
  }
  return crc;
}
