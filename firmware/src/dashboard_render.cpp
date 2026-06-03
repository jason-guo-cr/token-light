#include "dashboard_render.h"

static const int LCD_WIDTH = 400;

static void drawProgress(U8G2 &u8g2, int x, int y, int w, int percent) {
  int clamped = constrain(percent, 0, 100);
  int fill = (w - 4) * clamped / 100;
  u8g2.drawFrame(x, y, w, 10);
  u8g2.drawBox(x + 2, y + 2, fill, 6);
}

static void drawMetricCard(U8G2 &u8g2, int x, int y, const LimitWindow &window) {
  u8g2.drawRFrame(x, y, 168, 88, 6);
  u8g2.setFont(u8g2_font_9x18B_tf);
  u8g2.drawStr(x + 14, y + 24, window.label.c_str());

  u8g2.setFont(u8g2_font_6x13B_tf);
  int resetWidth = u8g2.getStrWidth(window.resetLabel.c_str());
  u8g2.drawStr(x + 154 - resetWidth, y + 22, window.resetLabel.c_str());

  u8g2.setFont(u8g2_font_logisoso26_tn);
  char percent[8];
  snprintf(percent, sizeof(percent), "%d%%", window.remainingPercent);
  u8g2.drawStr(x + 14, y + 58, percent);
  drawProgress(u8g2, x + 14, y + 70, 140, window.remainingPercent);
}

static void drawBattery(U8G2 &u8g2, int x, int y, int percent, bool charging) {
  int clamped = constrain(percent, 0, 100);
  u8g2.drawFrame(x, y, 24, 12);
  u8g2.drawBox(x + 24, y + 4, 3, 4);
  int fill = 20 * clamped / 100;
  u8g2.drawBox(x + 2, y + 2, fill, 8);
  if (charging) {
    u8g2.setDrawColor(0);
    u8g2.drawPixel(x + 10, y + 3);
    u8g2.drawPixel(x + 9, y + 4);
    u8g2.drawPixel(x + 11, y + 4);
    u8g2.drawPixel(x + 10, y + 5);
    u8g2.drawPixel(x + 10, y + 6);
    u8g2.drawPixel(x + 9, y + 7);
    u8g2.setDrawColor(1);
  }
}

static void drawTokenUsage(U8G2 &u8g2, const DisplaySnapshot &snapshot) {
  u8g2.drawFrame(20, 255, 360, 28);
  u8g2.setFont(u8g2_font_9x18B_tf);
  String line = "TOKEN DAY " + snapshot.tokenTodayLabel + "  WEEK " + snapshot.tokenWeekLabel;
  if (snapshot.tokenTodayLabel.length() == 0 || snapshot.tokenWeekLabel.length() == 0) {
    line = "TOKEN WAITING";
  }
  u8g2.drawStr(34, 276, line.c_str());
}

void renderDashboard(U8G2 &u8g2, const DisplaySnapshot &snapshot, unsigned long nowMs) {
  bool stale = snapshot.receivedAtMs > 0 && (nowMs - snapshot.receivedAtMs) > 300000UL;
  u8g2.clearBuffer();
  u8g2.setDrawColor(1);

  u8g2.setFont(u8g2_font_9x18B_tf);
  String topLeft = snapshot.date + " " + snapshot.weekday;
  u8g2.drawStr(16, 28, topLeft.c_str());
  if (snapshot.weatherDisplay.length() > 0) {
    u8g2.drawStr(134, 28, snapshot.weatherDisplay.c_str());
  }
  const char *state = stale ? "STALE" : (snapshot.status == "live" ? "LIVE" : "WAIT");
  u8g2.drawStr(246, 28, state);
  if (snapshot.batteryPercent >= 0) {
    char batteryText[8];
    snprintf(batteryText, sizeof(batteryText), "%d%%", snapshot.batteryPercent);
    u8g2.drawStr(304, 28, batteryText);
    drawBattery(u8g2, 360, 14, snapshot.batteryPercent, snapshot.batteryCharging);
  }

  u8g2.drawFrame(20, 40, 360, 72);
  u8g2.drawFrame(34, 51, 332, 48);
  u8g2.setFont(u8g2_font_logisoso50_tn);
  int clockWidth = u8g2.getStrWidth(snapshot.time.c_str());
  u8g2.drawStr((LCD_WIDTH - clockWidth) / 2, 100, snapshot.time.c_str());

  if (snapshot.limitUpdatedLabel.length() > 0) {
    String updated = "LIMIT UPDATED " + snapshot.limitUpdatedLabel;
    u8g2.setFont(u8g2_font_6x13B_tf);
    int updatedWidth = u8g2.getStrWidth(updated.c_str());
    u8g2.drawStr((LCD_WIDTH - updatedWidth) / 2, 130, updated.c_str());
  }

  if (snapshot.status == "live") {
    drawMetricCard(u8g2, 20, 140, snapshot.primary);
    drawMetricCard(u8g2, 212, 140, snapshot.secondary);
  } else {
    u8g2.drawFrame(20, 140, 360, 98);
    u8g2.setFont(u8g2_font_9x18B_tf);
    u8g2.drawStr(40, 176, snapshot.status.c_str());
    u8g2.setFont(u8g2_font_6x13_tf);
    u8g2.drawStr(40, 202, snapshot.message.c_str());
  }

  drawTokenUsage(u8g2, snapshot);

  u8g2.sendBuffer();
}
