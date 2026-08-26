#include "dashboard_render.h"

#include "page_renderers.h"

void renderDashboard(
    U8G2 &u8g2,
    const DisplaySnapshot &snapshot,
    const CompanionController &controller,
    const AmbientSnapshot &ambient,
    unsigned long nowMs,
    bool voiceOff) {
  u8g2.clearBuffer();
  u8g2.setDrawColor(1);
  switch (controller.page()) {
    case Page::Overview:
      renderOverviewPage(u8g2, snapshot, ambient, nowMs);
      break;
    case Page::Activity:
      renderActivityPage(u8g2, snapshot, nowMs);
      break;
    case Page::Focus:
      renderFocusPage(u8g2, snapshot, controller, nowMs);
      break;
  }
  if (voiceOff) {
    u8g2.setDrawColor(0);
    u8g2.drawBox(95, 116, 210, 65);
    u8g2.setDrawColor(1);
    u8g2.drawFrame(95, 116, 210, 65);
    u8g2.setFont(u8g2_font_9x18B_tf);
    const char *message = "VOICE OFF";
    u8g2.drawStr((400 - u8g2.getStrWidth(message)) / 2, 155, message);
  }
  u8g2.sendBuffer();
}
