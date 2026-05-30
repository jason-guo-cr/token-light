#include <Arduino.h>
#include "ST7305_U8g2.h"
#include "dashboard_render.h"

#define RLCD_SCK_PIN 11
#define RLCD_MOSI_PIN 12
#define RLCD_DC_PIN 5
#define RLCD_CS_PIN 40
#define RLCD_RST_PIN 41

static ST7305_U8g2 lcd(RLCD_SCK_PIN, RLCD_MOSI_PIN, RLCD_DC_PIN, RLCD_CS_PIN, RLCD_RST_PIN);
static U8G2 *u8g2 = nullptr;
static DisplaySnapshot snapshot;

static void setWindow(LimitWindow &window, const char *label, int remaining, int used, const char *resetLabel) {
  window.label = label;
  window.remainingPercent = remaining;
  window.usedPercent = used;
  window.resetLabel = resetLabel;
}

void setup() {
  Serial.begin(115200);
  delay(300);
  lcd.begin(0, U8G2_R1);
  u8g2 = lcd.getU8g2();

  snapshot.date = "05/30";
  snapshot.weekday = "SAT";
  snapshot.time = "15:50";
  snapshot.status = "live";
  setWindow(snapshot.primary, "5H LIMIT", 97, 3, "20:36");
  setWindow(snapshot.secondary, "WEEK LIMIT", 96, 4, "05/31");
  snapshot.receivedAtMs = millis();
}

void loop() {
  renderDashboard(*u8g2, snapshot, millis());
  delay(1000);
}
