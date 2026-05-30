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
  u8g2.setFont(u8g2_font_logisoso26_tn);
  char percent[8];
  snprintf(percent, sizeof(percent), "%d%%", window.remainingPercent);
  u8g2.drawStr(x + 14, y + 58, percent);
  u8g2.setFont(u8g2_font_9x18B_tf);
  u8g2.drawStr(x + 100, y + 56, "LEFT");
  drawProgress(u8g2, x + 14, y + 65, 140, window.remainingPercent);
  u8g2.setFont(u8g2_font_6x13B_tf);
  String reset = "RESET " + window.resetLabel;
  u8g2.drawStr(x + 14, y + 84, reset.c_str());
}

static void drawPixelPet(U8G2 &u8g2, int x, int y) {
  u8g2.drawFrame(x + 4, y + 4, 22, 18);
  u8g2.drawBox(x + 9, y + 10, 3, 3);
  u8g2.drawBox(x + 19, y + 10, 3, 3);
  u8g2.drawHLine(x + 11, y + 17, 9);
  u8g2.drawPixel(x + 2, y + 8);
  u8g2.drawPixel(x + 28, y + 8);
}

void renderDashboard(U8G2 &u8g2, const DisplaySnapshot &snapshot, unsigned long nowMs) {
  bool stale = snapshot.receivedAtMs > 0 && (nowMs - snapshot.receivedAtMs) > 300000UL;
  u8g2.clearBuffer();
  u8g2.setDrawColor(1);

  u8g2.setFont(u8g2_font_9x18B_tf);
  String topLeft = snapshot.date + " " + snapshot.weekday;
  u8g2.drawStr(16, 28, topLeft.c_str());
  u8g2.drawStr(310, 28, stale ? "STALE" : "LIVE");

  u8g2.drawFrame(14, 44, 372, 88);
  u8g2.drawFrame(26, 56, 348, 64);
  u8g2.setFont(u8g2_font_logisoso78_tn);
  int clockWidth = u8g2.getStrWidth(snapshot.time.c_str());
  u8g2.drawStr((LCD_WIDTH - clockWidth) / 2, 116, snapshot.time.c_str());

  if (snapshot.status == "live") {
    drawMetricCard(u8g2, 20, 150, snapshot.primary);
    drawMetricCard(u8g2, 212, 150, snapshot.secondary);
  } else {
    u8g2.drawFrame(20, 150, 360, 88);
    u8g2.setFont(u8g2_font_9x18B_tf);
    u8g2.drawStr(40, 186, snapshot.status.c_str());
    u8g2.setFont(u8g2_font_6x13_tf);
    u8g2.drawStr(40, 212, snapshot.message.c_str());
  }

  u8g2.drawFrame(20, 255, 160, 28);
  u8g2.drawFrame(196, 255, 184, 28);
  u8g2.setFont(u8g2_font_9x18B_tf);
  u8g2.drawStr(34, 276, stale ? "SYNC STALE" : "SYNC OK");
  drawPixelPet(u8g2, 330, 256);

  u8g2.sendBuffer();
}
