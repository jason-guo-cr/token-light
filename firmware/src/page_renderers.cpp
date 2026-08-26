#include "page_renderers.h"

#include "page_footer_labels.h"
#include "pet_sprites.h"

namespace {
constexpr int kLcdWidth = 400;

void drawCentered(U8G2 &u8g2, int y, const String &text) {
  u8g2.drawStr((kLcdWidth - u8g2.getStrWidth(text.c_str())) / 2, y, text.c_str());
}

String elapsedLabel(uint32_t seconds) {
  char value[16];
  snprintf(value, sizeof(value), "%02lu:%02lu", static_cast<unsigned long>(seconds / 60U),
           static_cast<unsigned long>(seconds % 60U));
  return value;
}

void drawTopStatus(U8G2 &u8g2, const DisplaySnapshot &snapshot, uint32_t nowMs) {
  const bool stale = snapshot.receivedAtMs > 0 && nowMs - snapshot.receivedAtMs > 300000UL;
  u8g2.setFont(u8g2_font_6x13B_tf);
  String left = snapshot.date + " " + snapshot.weekday;
  u8g2.drawStr(12, 20, left.c_str());
  if (snapshot.weatherDisplay.length() > 0) {
    u8g2.drawStr(116, 20, snapshot.weatherDisplay.c_str());
  }
  const char *state = stale ? "STALE" : (snapshot.status == "live" ? "LIVE" : (snapshot.status == "cached" ? "CACHE" : "WAIT"));
  u8g2.drawStr(292, 20, state);
  if (snapshot.batteryPercent >= 0) {
    char battery[8];
    snprintf(battery, sizeof(battery), "%d%%", snapshot.batteryPercent);
    u8g2.drawStr(354, 20, battery);
  }
}

bool beforeDeadline(bool active, uint32_t nowMs, uint32_t deadlineMs) {
  return active && static_cast<int32_t>(deadlineMs - nowMs) > 0;
}

String visiblePetPose(const DisplaySnapshot &snapshot, bool focusRunning, uint32_t nowMs) {
  if (snapshot.petPose == "alert") return "alert";
  if (beforeDeadline(snapshot.celebrationActive, nowMs, snapshot.celebrationUntilMs)) {
    return "celebrate";
  }
  if (focusRunning) return "focus";
  if (snapshot.petPose == "celebrate") {
    return snapshot.primary.valid && snapshot.primary.remainingPercent <= 10 ? "tired" : "sleep";
  }
  return snapshot.petPose;
}
}

void renderOverviewPage(
    U8G2 &u8g2, const DisplaySnapshot &snapshot, const AmbientSnapshot &ambient, uint32_t nowMs,
    bool threeKeyProfile) {
  drawTopStatus(u8g2, snapshot, nowMs);

  u8g2.drawFrame(20, 33, 360, 69);
  u8g2.setFont(u8g2_font_logisoso50_tn);
  drawCentered(u8g2, 91, snapshot.time);

  u8g2.setFont(u8g2_font_9x18B_tf);
  String quota;
  if (snapshot.primary.valid) {
    quota = snapshot.primary.label + "  " + String(snapshot.primary.remainingPercent) + "% LEFT  " + snapshot.forecastLabel;
  } else if (snapshot.status == "api_error" && snapshot.message.length() > 0) {
    quota = "CODEX WEEK  " + snapshot.message;
  } else {
    quota = "CODEX WEEK  --% LEFT  EST --";
  }
  u8g2.drawStr(20, 132, quota.c_str());
  u8g2.drawFrame(20, 143, 360, 12);
  const int quotaWidth = snapshot.primary.valid
                             ? 356 * constrain(snapshot.primary.remainingPercent, 0, 100) / 100
                             : 0;
  u8g2.drawBox(22, 145, quotaWidth, 8);

  String burn = "BURN " + snapshot.tokenBurnLabel + "  PACE " + snapshot.paceLabel;
  u8g2.drawStr(20, 187, burn.c_str());
  u8g2.setFont(u8g2_font_6x13B_tf);
  String indoor = "IN --C --%";
  if (ambient.valid) {
    indoor = "IN " + String(ambient.temperatureC) + "C " + String(ambient.humidityPercent) + "%";
    if (ambient.cached) indoor += " CACHE";
  }
  u8g2.drawStr(20, 220, indoor.c_str());
  String token = "DAY " + snapshot.tokenTodayLabel + "  WEEK " + snapshot.tokenWeekLabel;
  u8g2.drawStr(20, 249, token.c_str());
  drawPet(u8g2, 332, 209, 3, visiblePetPose(snapshot, false, nowMs), nowMs);

  u8g2.setFont(u8g2_font_5x8_tf);
  u8g2.drawStr(20, 287, navigationFooterLabels(threeKeyProfile).primary);
}

void renderActivityPage(
    U8G2 &u8g2, const DisplaySnapshot &snapshot, uint32_t nowMs, bool threeKeyProfile) {
  u8g2.setFont(u8g2_font_9x18B_tf);
  u8g2.drawStr(18, 26, "CODEX NOW");
  const bool stale = snapshot.receivedAtMs > 0 && nowMs - snapshot.receivedAtMs > 300000UL;
  u8g2.drawStr(330, 26, stale ? "STALE" : "LIVE");

  u8g2.setFont(u8g2_font_logisoso32_tf);
  drawCentered(u8g2, 83, snapshot.activityLabel);
  u8g2.setFont(u8g2_font_9x18B_tf);
  drawCentered(u8g2, 113, snapshot.activityDetail);
  u8g2.setFont(u8g2_font_logisoso20_tn);
  drawCentered(u8g2, 144, elapsedLabel(snapshot.activityElapsedSeconds));

  drawPet(u8g2, 168, 157, 4, visiblePetPose(snapshot, false, nowMs), nowMs);
  u8g2.setFont(u8g2_font_6x13B_tf);
  String week = snapshot.primary.valid ? String(snapshot.primary.remainingPercent) + "%" : "--%";
  String footer = "BURN " + snapshot.tokenBurnLabel + "  WEEK " + week + "  " + snapshot.forecastLabel;
  drawCentered(u8g2, 259, footer);
  u8g2.setFont(u8g2_font_5x8_tf);
  drawCentered(u8g2, 289, navigationFooterLabels(threeKeyProfile).primary);
}

void renderFocusPage(
    U8G2 &u8g2, const DisplaySnapshot &snapshot, const CompanionController &controller,
    uint32_t nowMs, bool threeKeyProfile) {
  u8g2.setFont(u8g2_font_9x18B_tf);
  u8g2.drawStr(18, 26, controller.focusPhase() == FocusPhase::Focus ? "FOCUS" : "BREAK");
  String phaseDuration = controller.focusPhase() == FocusPhase::Focus ? "25:00" : "05:00";
  u8g2.drawStr(330, 26, phaseDuration.c_str());

  u8g2.setFont(u8g2_font_logisoso50_tn);
  drawCentered(u8g2, 103, elapsedLabel(controller.remainingSeconds()));
  u8g2.setFont(u8g2_font_9x18B_tf);
  const char *state = controller.focusRunState() == FocusRunState::Running
                          ? "RUNNING"
                          : (controller.focusRunState() == FocusRunState::Paused ? "PAUSED" : "READY");
  drawCentered(u8g2, 136, state);

  const bool running = controller.focusRunState() == FocusRunState::Running;
  drawPet(u8g2, 168, 153, 4, visiblePetPose(snapshot, running, nowMs), nowMs);
  u8g2.setFont(u8g2_font_6x13B_tf);
  drawCentered(u8g2, 246, controller.focusPhase() == FocusPhase::Focus ? "DEEP WORK" : "TAKE A BREAK");
  u8g2.setFont(u8g2_font_5x8_tf);
  const PageFooterLabels footer = focusFooterLabels(threeKeyProfile);
  if (footer.secondary != nullptr) {
    drawCentered(u8g2, 278, footer.primary);
    drawCentered(u8g2, 291, footer.secondary);
  } else {
    drawCentered(u8g2, 289, footer.primary);
  }
}
