#pragma once

#include <Arduino.h>
#include <U8g2lib.h>

struct PetFrames {
  const uint8_t *first;
  const uint8_t *second;
};

// Every pose keeps the same ear, face, body, paw, and right-side tail cues so
// the 8x8 character still reads as one cat when scaled on each page.
static const uint8_t PET_SLEEP_0[] PROGMEM = {0x21, 0x33, 0x3F, 0x2D, 0x33, 0x5E, 0x7E, 0x32};
static const uint8_t PET_SLEEP_1[] PROGMEM = {0x21, 0x33, 0x3F, 0x3F, 0x33, 0x5E, 0x3E, 0x32};
static const uint8_t PET_WORK_0[] PROGMEM = {0x21, 0x33, 0x3F, 0x2D, 0x3F, 0x5F, 0x7E, 0x34};
static const uint8_t PET_WORK_1[] PROGMEM = {0x21, 0x33, 0x3F, 0x2D, 0x3F, 0x5E, 0x7F, 0x32};
static const uint8_t PET_CODE_0[] PROGMEM = {0x21, 0x33, 0x3F, 0x2D, 0x33, 0x7E, 0x5F, 0x36};
static const uint8_t PET_CODE_1[] PROGMEM = {0x21, 0x33, 0x3F, 0x2D, 0x33, 0x7E, 0x7D, 0x1A};
static const uint8_t PET_TEST_0[] PROGMEM = {0x21, 0x3F, 0x3F, 0x2D, 0x3F, 0x5E, 0x7E, 0x2A};
static const uint8_t PET_TEST_1[] PROGMEM = {0x21, 0x33, 0x3F, 0x3F, 0x2D, 0x5E, 0x7E, 0x32};
static const uint8_t PET_WAIT_0[] PROGMEM = {0x21, 0x33, 0x3F, 0x21, 0x3F, 0x5E, 0x7E, 0x24};
static const uint8_t PET_WAIT_1[] PROGMEM = {0x21, 0x33, 0x3F, 0x21, 0x3F, 0x9E, 0x7E, 0x24};
static const uint8_t PET_FOCUS_0[] PROGMEM = {0x21, 0x33, 0x3F, 0x33, 0x3F, 0x5E, 0x7E, 0x34};
static const uint8_t PET_FOCUS_1[] PROGMEM = {0x21, 0x33, 0x3F, 0x33, 0x3F, 0x5F, 0x7E, 0x32};
static const uint8_t PET_TIRED_0[] PROGMEM = {0x21, 0x33, 0x3F, 0x21, 0x33, 0x3C, 0x7E, 0x32};
static const uint8_t PET_TIRED_1[] PROGMEM = {0x21, 0x33, 0x3F, 0x3F, 0x33, 0x3C, 0x3E, 0x32};
static const uint8_t PET_CELEBRATE_0[] PROGMEM = {0x21, 0xB3, 0xFF, 0x2D, 0x3F, 0x7E, 0x5A, 0x24};
static const uint8_t PET_CELEBRATE_1[] PROGMEM = {0xA1, 0xB3, 0xFF, 0x2D, 0x3F, 0x7E, 0x5A, 0x42};
static const uint8_t PET_ALERT_0[] PROGMEM = {0xA1, 0xB3, 0xFF, 0x21, 0x3F, 0x5E, 0x7E, 0x32};
static const uint8_t PET_ALERT_1[] PROGMEM = {0x61, 0x73, 0x7F, 0x21, 0x3F, 0x5E, 0x7E, 0x32};

inline PetFrames petFrames(const String &pose) {
  if (pose == "working") return {PET_WORK_0, PET_WORK_1};
  if (pose == "coding") return {PET_CODE_0, PET_CODE_1};
  if (pose == "testing") return {PET_TEST_0, PET_TEST_1};
  if (pose == "waiting") return {PET_WAIT_0, PET_WAIT_1};
  if (pose == "focus") return {PET_FOCUS_0, PET_FOCUS_1};
  if (pose == "tired") return {PET_TIRED_0, PET_TIRED_1};
  if (pose == "celebrate") return {PET_CELEBRATE_0, PET_CELEBRATE_1};
  if (pose == "alert") return {PET_ALERT_0, PET_ALERT_1};
  return {PET_SLEEP_0, PET_SLEEP_1};
}

inline void drawPet(U8G2 &u8g2, int x, int y, int scale, const String &pose, uint32_t nowMs) {
  const PetFrames frames = petFrames(pose);
  const uint8_t *pixels = ((nowMs / 1000U) & 1U) == 0 ? frames.first : frames.second;
  for (uint8_t row = 0; row < 8; ++row) {
    const uint8_t bits = pgm_read_byte(pixels + row);
    for (uint8_t column = 0; column < 8; ++column) {
      if (bits & (1U << column)) {
        u8g2.drawBox(x + column * scale, y + row * scale, scale, scale);
      }
    }
  }
}
