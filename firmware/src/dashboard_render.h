#pragma once

#include <U8g2lib.h>
#include "ambient_model.h"
#include "companion_controller.h"
#include "dashboard_state.h"

void renderDashboard(
    U8G2 &u8g2,
    const DisplaySnapshot &snapshot,
    const CompanionController &controller,
    const AmbientSnapshot &ambient,
    unsigned long nowMs,
    bool voiceOff);
