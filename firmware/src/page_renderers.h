#pragma once

#include <U8g2lib.h>

#include "ambient_model.h"
#include "companion_controller.h"
#include "dashboard_state.h"

void renderOverviewPage(
    U8G2 &u8g2, const DisplaySnapshot &snapshot, const AmbientSnapshot &ambient, uint32_t nowMs);
void renderActivityPage(U8G2 &u8g2, const DisplaySnapshot &snapshot, uint32_t nowMs);
void renderFocusPage(
    U8G2 &u8g2, const DisplaySnapshot &snapshot, const CompanionController &controller, uint32_t nowMs);
