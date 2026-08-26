#include "dashboard_render.h"

#include "page_renderers.h"

namespace {
const char *feedbackLabel(ActionFeedback feedback) {
  switch (feedback) {
    case ActionFeedback::PageOverview:
      return "PAGE: OVERVIEW";
    case ActionFeedback::PageActivity:
      return "PAGE: CODEX NOW";
    case ActionFeedback::PageFocus:
      return "PAGE: FOCUS";
    case ActionFeedback::FocusStarted:
      return "FOCUS STARTED";
    case ActionFeedback::FocusPaused:
      return "FOCUS PAUSED";
    case ActionFeedback::FocusResumed:
      return "FOCUS RESUMED";
    case ActionFeedback::FocusReset:
      return "FOCUS RESET";
    case ActionFeedback::VoiceOff:
      return "VOICE OFF";
    case ActionFeedback::None:
      return nullptr;
  }
  return nullptr;
}
}

void renderDashboard(
    U8G2 &u8g2,
    const DisplaySnapshot &snapshot,
    const CompanionController &controller,
    const AmbientSnapshot &ambient,
    unsigned long nowMs,
    bool threeKeyProfile,
    bool actionFeedbackActive) {
  u8g2.clearBuffer();
  u8g2.setDrawColor(1);
  switch (controller.page()) {
    case Page::Overview:
      renderOverviewPage(u8g2, snapshot, ambient, nowMs, threeKeyProfile);
      break;
    case Page::Activity:
      renderActivityPage(u8g2, snapshot, nowMs, threeKeyProfile);
      break;
    case Page::Focus:
      renderFocusPage(u8g2, snapshot, controller, nowMs, threeKeyProfile);
      break;
  }
  const char *message = actionFeedbackActive ? feedbackLabel(controller.actionFeedback()) : nullptr;
  if (message != nullptr) {
    u8g2.setDrawColor(0);
    u8g2.drawBox(95, 116, 210, 65);
    u8g2.setDrawColor(1);
    u8g2.drawFrame(95, 116, 210, 65);
    u8g2.setFont(u8g2_font_9x18B_tf);
    u8g2.drawStr((400 - u8g2.getStrWidth(message)) / 2, 155, message);
  }
  u8g2.sendBuffer();
}
