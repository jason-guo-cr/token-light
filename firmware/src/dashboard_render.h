#pragma once

#include <U8g2lib.h>
#include "dashboard_state.h"

void renderDashboard(U8G2 &u8g2, const DisplaySnapshot &snapshot, unsigned long nowMs);
