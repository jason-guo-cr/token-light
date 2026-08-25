#include "audio_driver.h"

#include <driver/i2s.h>
#include <math.h>

namespace {
constexpr uint8_t kCodecAddress = 0x18;
constexpr int kPaPin = 46;
constexpr i2s_port_t kI2sPort = I2S_NUM_0;
constexpr uint32_t kSampleRate = 16000;
constexpr uint32_t kToneFrames = 9600;
constexpr size_t kChunkFrames = 64;
}

AudioDriver::AudioDriver(TwoWire &wire)
    : wire_(wire), ready_(false), active_(false), frameIndex_(0), startedAtMs_(0) {}

bool AudioDriver::begin() {
  pinMode(kPaPin, OUTPUT);
  digitalWrite(kPaPin, LOW);

  i2s_config_t config = {};
  config.mode = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_TX);
  config.sample_rate = kSampleRate;
  config.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
  config.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
  config.communication_format = I2S_COMM_FORMAT_STAND_I2S;
  config.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
  config.dma_buf_count = 4;
  config.dma_buf_len = 128;
  config.use_apll = true;
  config.tx_desc_auto_clear = true;
  config.fixed_mclk = kSampleRate * 256;

  i2s_pin_config_t pins = {};
  pins.mck_io_num = 16;
  pins.bck_io_num = 9;
  pins.ws_io_num = 45;
  pins.data_out_num = 8;
  pins.data_in_num = 10;
  if (i2s_driver_install(kI2sPort, &config, 0, nullptr) != ESP_OK ||
      i2s_set_pin(kI2sPort, &pins) != ESP_OK ||
      i2s_set_clk(kI2sPort, kSampleRate, I2S_BITS_PER_SAMPLE_16BIT, I2S_CHANNEL_STEREO) != ESP_OK) {
    return false;
  }
  ready_ = initializeCodec();
  return ready_;
}

bool AudioDriver::playCompletion() {
  if (!ready_ || active_) {
    return false;
  }
  frameIndex_ = 0;
  startedAtMs_ = millis();
  active_ = true;
  digitalWrite(kPaPin, HIGH);
  return true;
}

void AudioDriver::tick() {
  if (!active_) {
    return;
  }
  if (millis() - startedAtMs_ >= 1500U) {
    active_ = false;
    digitalWrite(kPaPin, LOW);
    return;
  }
  int16_t samples[kChunkFrames * 2];
  const size_t frames = min(static_cast<uint32_t>(kChunkFrames), kToneFrames - frameIndex_);
  for (size_t index = 0; index < frames; ++index) {
    const uint32_t frame = frameIndex_ + index;
    const float frequency = frame < kToneFrames / 2 ? 880.0f : 1174.66f;
    const float phase = 2.0f * PI * frequency * static_cast<float>(frame) / kSampleRate;
    const float envelope = min(1.0f, min(static_cast<float>(frame) / 320.0f,
                                         static_cast<float>(kToneFrames - frame) / 640.0f));
    const int16_t sample = static_cast<int16_t>(sinf(phase) * 4200.0f * envelope);
    samples[index * 2] = sample;
    samples[index * 2 + 1] = sample;
  }

  size_t bytesWritten = 0;
  if (i2s_write(kI2sPort, samples, frames * 2 * sizeof(int16_t), &bytesWritten, 0) != ESP_OK) {
    active_ = false;
    digitalWrite(kPaPin, LOW);
    return;
  }
  frameIndex_ += bytesWritten / (2 * sizeof(int16_t));
  if (frameIndex_ >= kToneFrames) {
    active_ = false;
    digitalWrite(kPaPin, LOW);
  }
}

bool AudioDriver::active() const { return active_; }

bool AudioDriver::writeRegister(uint8_t reg, uint8_t value) {
  wire_.beginTransmission(kCodecAddress);
  wire_.write(reg);
  wire_.write(value);
  return wire_.endTransmission() == 0;
}

bool AudioDriver::readRegister(uint8_t reg, uint8_t &value) {
  wire_.beginTransmission(kCodecAddress);
  wire_.write(reg);
  if (wire_.endTransmission(false) != 0 || wire_.requestFrom(kCodecAddress, static_cast<uint8_t>(1)) != 1) {
    return false;
  }
  value = static_cast<uint8_t>(wire_.read());
  return true;
}

bool AudioDriver::initializeCodec() {
  bool ok = true;
  ok &= writeRegister(0x44, 0x08);
  ok &= writeRegister(0x44, 0x08);
  ok &= writeRegister(0x01, 0x30);
  ok &= writeRegister(0x02, 0x00);
  ok &= writeRegister(0x03, 0x10);
  ok &= writeRegister(0x16, 0x24);
  ok &= writeRegister(0x04, 0x10);
  ok &= writeRegister(0x05, 0x00);
  ok &= writeRegister(0x0B, 0x00);
  ok &= writeRegister(0x0C, 0x00);
  ok &= writeRegister(0x10, 0x1F);
  ok &= writeRegister(0x11, 0x7F);
  ok &= writeRegister(0x00, 0x80);
  ok &= writeRegister(0x01, 0x3F);
  ok &= writeRegister(0x13, 0x10);
  ok &= writeRegister(0x1B, 0x0A);
  ok &= writeRegister(0x1C, 0x6A);
  ok &= writeRegister(0x44, 0x58);

  uint8_t interfaceValue = 0;
  ok &= readRegister(0x09, interfaceValue);
  ok &= writeRegister(0x09, static_cast<uint8_t>((interfaceValue & 0xE0) | 0x0C));
  ok &= writeRegister(0x0A, 0x4C);
  ok &= writeRegister(0x02, 0x00);
  ok &= writeRegister(0x05, 0x00);
  ok &= writeRegister(0x03, 0x10);
  ok &= writeRegister(0x04, 0x20);
  ok &= writeRegister(0x07, 0x00);
  ok &= writeRegister(0x08, 0xFF);
  ok &= writeRegister(0x06, 0x03);

  ok &= writeRegister(0x00, 0x80);
  ok &= writeRegister(0x01, 0x3F);
  ok &= writeRegister(0x09, 0x0C);
  ok &= writeRegister(0x0A, 0x4C);
  ok &= writeRegister(0x17, 0xBF);
  ok &= writeRegister(0x0E, 0x02);
  ok &= writeRegister(0x12, 0x00);
  ok &= writeRegister(0x14, 0x1A);
  ok &= writeRegister(0x0D, 0x01);
  ok &= writeRegister(0x15, 0x40);
  ok &= writeRegister(0x37, 0x08);
  ok &= writeRegister(0x45, 0x00);
  ok &= writeRegister(0x31, 0x00);
  ok &= writeRegister(0x32, 0xB8);
  return ok;
}
