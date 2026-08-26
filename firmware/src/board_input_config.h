#pragma once

#include <stdint.h>

#define TOKEN_LIGHT_PROFILE_LEGACY_SINGLE_KEY 0
#define TOKEN_LIGHT_PROFILE_THREE_KEY 1

#ifndef TOKEN_LIGHT_INPUT_PROFILE
#define TOKEN_LIGHT_INPUT_PROFILE TOKEN_LIGHT_PROFILE_LEGACY_SINGLE_KEY
#endif

#if TOKEN_LIGHT_INPUT_PROFILE != TOKEN_LIGHT_PROFILE_LEGACY_SINGLE_KEY && \
    TOKEN_LIGHT_INPUT_PROFILE != TOKEN_LIGHT_PROFILE_THREE_KEY
#error "TOKEN_LIGHT_INPUT_PROFILE must select legacy_single_key or three_key"
#endif

namespace BoardInputConfig {

constexpr uint8_t kLegacyKeyPin = 18;
constexpr bool kThreeKeyEnabled = TOKEN_LIGHT_INPUT_PROFILE == TOKEN_LIGHT_PROFILE_THREE_KEY;

#if TOKEN_LIGHT_INPUT_PROFILE == TOKEN_LIGHT_PROFILE_THREE_KEY
#if !defined(TOKEN_LIGHT_LEFT_KEY_PIN) || !defined(TOKEN_LIGHT_CENTER_KEY_PIN) || \
    !defined(TOKEN_LIGHT_RIGHT_KEY_PIN)
#error "three_key requires explicitly verified LEFT/CENTER/RIGHT GPIO definitions"
#endif

#ifndef TOKEN_LIGHT_THREE_KEY_EXTERNAL_PULLUPS
#define TOKEN_LIGHT_THREE_KEY_EXTERNAL_PULLUPS 0
#endif

constexpr uint8_t kLeftKeyPin = TOKEN_LIGHT_LEFT_KEY_PIN;
constexpr uint8_t kCenterKeyPin = TOKEN_LIGHT_CENTER_KEY_PIN;
constexpr uint8_t kRightKeyPin = TOKEN_LIGHT_RIGHT_KEY_PIN;
constexpr bool kThreeKeyExternalPullups = TOKEN_LIGHT_THREE_KEY_EXTERNAL_PULLUPS != 0;

static_assert(kLeftKeyPin != kCenterKeyPin && kLeftKeyPin != kRightKeyPin &&
                  kCenterKeyPin != kRightKeyPin,
              "three_key GPIO definitions must be independent");
#endif

}  // namespace BoardInputConfig
