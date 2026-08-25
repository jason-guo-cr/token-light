#pragma once

#include <Arduino.h>
#include <Wire.h>

class AudioDriver {
 public:
  explicit AudioDriver(TwoWire &wire);

  bool begin();
  bool playCompletion();
  void tick();
  bool active() const;

 private:
  bool writeRegister(uint8_t reg, uint8_t value);
  bool readRegister(uint8_t reg, uint8_t &value);
  bool initializeCodec();

  TwoWire &wire_;
  bool ready_;
  bool active_;
  uint32_t frameIndex_;
  uint32_t startedAtMs_;
};
