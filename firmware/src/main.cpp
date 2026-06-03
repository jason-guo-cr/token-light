#include <Arduino.h>
#include <ArduinoJson.h>
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
static String lineBuffer;
static unsigned long lastRenderMs = 0;

static void setWindow(LimitWindow &window, const char *label, int remaining, int used, const char *resetLabel) {
  window.label = label;
  window.remainingPercent = remaining;
  window.usedPercent = used;
  window.resetLabel = resetLabel;
}

static void setBootSnapshot() {
  snapshot.date = "BOOT";
  snapshot.weekday = "DBG";
  snapshot.time = "00:00";
  snapshot.status = "boot";
  snapshot.message = "WAITING FOR HOST";
  snapshot.batteryPercent = -1;
  snapshot.batteryCharging = false;
  snapshot.tokenTodayLabel = "";
  snapshot.tokenWeekLabel = "";
  snapshot.limitUpdatedLabel = "";
  snapshot.weatherDisplay = "";
  setWindow(snapshot.primary, "5H LIMIT", 0, 0, "--:--");
  setWindow(snapshot.secondary, "WEEK LIMIT", 0, 0, "--/--");
  snapshot.receivedAtMs = 0;
}

static LimitWindow readWindow(JsonObject obj) {
  LimitWindow window;
  window.label = obj["label"] | "";
  window.remainingPercent = obj["remaining_percent"] | 0;
  window.usedPercent = obj["used_percent"] | 0;
  window.resetLabel = obj["reset_label"] | "";
  return window;
}

static void applySnapshot(JsonDocument &doc) {
  const char *type = doc["type"] | "";
  if (strcmp(type, "snapshot") != 0) {
    Serial.println("token-light: ignored non-snapshot JSON");
    return;
  }

  snapshot.date = doc["date"] | snapshot.date;
  snapshot.weekday = doc["weekday"] | snapshot.weekday;
  snapshot.time = doc["time"] | snapshot.time;
  snapshot.status = doc["status"] | "api_error";
  snapshot.message = doc["message"] | "";
  snapshot.limitUpdatedLabel = doc["limit_updated_label"] | snapshot.limitUpdatedLabel;
  JsonObject battery = doc["battery"].as<JsonObject>();
  if (!battery.isNull()) {
    snapshot.batteryPercent = battery["percent"] | -1;
    snapshot.batteryCharging = battery["charging"] | false;
  }
  JsonObject tokenUsage = doc["token_usage"].as<JsonObject>();
  if (!tokenUsage.isNull()) {
    snapshot.tokenTodayLabel = tokenUsage["today_label"] | "";
    snapshot.tokenWeekLabel = tokenUsage["week_label"] | "";
  }
  JsonObject weather = doc["weather"].as<JsonObject>();
  if (!weather.isNull()) {
    snapshot.weatherDisplay = weather["display"] | "";
  }

  if (snapshot.status == "live") {
    snapshot.primary = readWindow(doc["primary"].as<JsonObject>());
    snapshot.secondary = readWindow(doc["secondary"].as<JsonObject>());
  }
  snapshot.receivedAtMs = millis();
  renderDashboard(*u8g2, snapshot, snapshot.receivedAtMs);
  lastRenderMs = snapshot.receivedAtMs;
  Serial.printf("token-light: snapshot status=%s time=%s primary=%d secondary=%d\n",
                snapshot.status.c_str(),
                snapshot.time.c_str(),
                snapshot.primary.remainingPercent,
                snapshot.secondary.remainingPercent);
}

static void processSerialLine(const String &line) {
  Serial.printf("token-light: rx line length=%u\n", (unsigned)line.length());
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, line);
  if (error) {
    Serial.printf("token-light: json error=%s\n", error.c_str());
    return;
  }
  applySnapshot(doc);
}

static void readSerialInput() {
  while (Serial.available() > 0) {
    char ch = static_cast<char>(Serial.read());
    if (ch == '\n') {
      processSerialLine(lineBuffer);
      lineBuffer = "";
    } else if (ch != '\r' && lineBuffer.length() < 1200) {
      lineBuffer += ch;
    }
  }
}

void setup() {
  Serial.setRxBufferSize(2048);
  Serial.begin(115200);
  delay(1000);
  Serial.println("token-light: boot");
  lcd.begin(0, U8G2_R1);
  u8g2 = lcd.getU8g2();
  Serial.println("token-light: lcd ready");
  setBootSnapshot();
  renderDashboard(*u8g2, snapshot, millis());
  Serial.println("token-light: boot screen rendered");
}

void loop() {
  readSerialInput();
  unsigned long now = millis();
  if (now - lastRenderMs >= 1000UL) {
    renderDashboard(*u8g2, snapshot, now);
    lastRenderMs = now;
  }
  delay(5);
}
