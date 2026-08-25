#include <Arduino.h>
#include <ArduinoJson.h>
#include <Wire.h>

#include "ST7305_U8g2.h"
#include "ambient_model.h"
#include "audio_driver.h"
#include "companion_controller.h"
#include "completion_notifier.h"
#include "dashboard_render.h"
#include "key_gesture.h"
#include "shtc3_reader.h"
#include "snapshot_protocol.h"

#define RLCD_SCK_PIN 11
#define RLCD_MOSI_PIN 12
#define RLCD_DC_PIN 5
#define RLCD_CS_PIN 40
#define RLCD_RST_PIN 41
#define KEY_PIN 18
#define I2C_SDA_PIN 13
#define I2C_SCL_PIN 14

static ST7305_U8g2 lcd(RLCD_SCK_PIN, RLCD_MOSI_PIN, RLCD_DC_PIN, RLCD_CS_PIN, RLCD_RST_PIN);
static U8G2 *u8g2 = nullptr;
static DisplaySnapshot snapshot;
static SnapshotProtocolState protocolState;
static CompanionController companionController;
static KeyGestureDetector keyGesture(30, 350, 800);
static AmbientModel ambientModel(300000);
static Shtc3Reader shtc3(Wire);
static CompletionNotifier completionNotifier;
static AudioDriver audioDriver(Wire);
static String lineBuffer;
static unsigned long lastRenderMs = 0;
static unsigned long voiceOffUntilMs = 0;

static bool beforeDeadline(uint32_t nowMs, uint32_t deadlineMs) {
  return static_cast<int32_t>(deadlineMs - nowMs) > 0;
}

static void syncProtocolState() {
  snapshot.date = protocolState.date.c_str();
  snapshot.weekday = protocolState.weekday.c_str();
  snapshot.time = protocolState.time.c_str();
  snapshot.status = protocolState.status.c_str();
  snapshot.message = protocolState.message.c_str();
  snapshot.batteryPercent = protocolState.batteryPercent;
  snapshot.batteryCharging = protocolState.batteryCharging;
  snapshot.tokenTodayLabel = protocolState.tokenTodayLabel.c_str();
  snapshot.tokenWeekLabel = protocolState.tokenWeekLabel.c_str();
  snapshot.tokenBurnLabel = protocolState.tokenBurnLabel.c_str();
  snapshot.forecastLabel = protocolState.forecastLabel.c_str();
  snapshot.paceLabel = protocolState.paceLabel.c_str();
  snapshot.limitUpdatedLabel = protocolState.limitUpdatedLabel.c_str();
  snapshot.weatherDisplay = protocolState.weatherDisplay.c_str();
  snapshot.activityState = protocolState.activityState.c_str();
  snapshot.activityLabel = protocolState.activityLabel.c_str();
  snapshot.activityDetail = protocolState.activityDetail.c_str();
  snapshot.activityElapsedSeconds = protocolState.activityElapsedSeconds;
  snapshot.completionSequence = protocolState.completionSequence;
  snapshot.petPose = protocolState.petPose.c_str();
  snapshot.audioEnabled = protocolState.audioEnabled;
  snapshot.audioQuiet = protocolState.audioQuiet;
  snapshot.primary.label = protocolState.primary.label.c_str();
  snapshot.primary.remainingPercent = protocolState.primary.remainingPercent;
  snapshot.primary.usedPercent = protocolState.primary.usedPercent;
  snapshot.primary.resetLabel = protocolState.primary.resetLabel.c_str();
  snapshot.primary.valid = protocolState.primary.valid;
}

static void setBootSnapshot() {
  protocolState = SnapshotProtocolState();
  syncProtocolState();
  snapshot.receivedAtMs = 0;
}

static int snapshotHour() {
  if (snapshot.time.length() < 2 || !isDigit(snapshot.time[0]) || !isDigit(snapshot.time[1])) return 12;
  return (snapshot.time[0] - '0') * 10 + (snapshot.time[1] - '0');
}

static bool applySnapshot(JsonDocument &doc) {
  const SnapshotProtocolResult result = applySnapshotProtocol(doc, protocolState);
  if (!result.accepted) {
    Serial.println("token-light: ignored non-snapshot JSON");
    return false;
  }
  syncProtocolState();
  snapshot.receivedAtMs = millis();
  return result.hasCompletionSequence;
}

static void renderNow(uint32_t nowMs) {
  renderDashboard(*u8g2, snapshot, companionController, ambientModel.snapshot(nowMs), nowMs,
                  beforeDeadline(nowMs, voiceOffUntilMs));
  lastRenderMs = nowMs;
}

static void processSerialLine(const String &line) {
  JsonDocument doc;
  const DeserializationError error = deserializeJson(doc, line);
  if (error) {
    Serial.printf("token-light: json error=%s\n", error.c_str());
    return;
  }
  const bool hasCompletionSequence = applySnapshot(doc);
  const uint32_t nowMs = millis();
  if (hasCompletionSequence) {
    const bool newCompletion = completionNotifier.observe(snapshot.completionSequence, nowMs);
    const int hour = snapshotHour();
    const bool quietHour = hour >= 22 || hour < 8;
    const bool audioAlways = quietHour && !snapshot.audioQuiet;
    snapshot.celebrationActive = completionNotifier.hasCelebration();
    snapshot.celebrationUntilMs = completionNotifier.celebrationUntilMs();
    if (newCompletion && completionNotifier.shouldPlay(hour, snapshot.audioEnabled, audioAlways)) {
      audioDriver.playCompletion();
    }
  }
  renderNow(nowMs);
}

static void readSerialInput() {
  while (Serial.available() > 0) {
    const char ch = static_cast<char>(Serial.read());
    if (ch == '\n') {
      processSerialLine(lineBuffer);
      lineBuffer = "";
    } else if (ch != '\r') {
      if (lineBuffer.length() < 1200) {
        lineBuffer += ch;
      } else {
        lineBuffer = "";
      }
    }
  }
}

static bool handleKey(uint32_t nowMs) {
  const KeyGesture gesture = keyGesture.update(digitalRead(KEY_PIN) == LOW, nowMs);
  switch (gesture) {
    case KeyGesture::Short:
      companionController.onShortPress();
      return true;
    case KeyGesture::Double:
      companionController.onDoublePress(nowMs);
      return true;
    case KeyGesture::Long:
      if (companionController.onLongPress()) {
        voiceOffUntilMs = nowMs + 1200;
      }
      return true;
    case KeyGesture::None:
      return false;
  }
  return false;
}

void setup() {
  Serial.setRxBufferSize(2048);
  Serial.begin(115200);
  delay(1000);
  pinMode(KEY_PIN, INPUT_PULLUP);
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  Wire.setClock(400000);
  Wire.setTimeOut(20);

  lcd.begin(0, U8G2_R1);
  u8g2 = lcd.getU8g2();
  setBootSnapshot();
  shtc3.begin(millis());
  const bool audioReady = audioDriver.begin();
  Serial.printf("token-light: audio %s\n", audioReady ? "ready" : "unavailable");
  renderNow(millis());
}

void loop() {
  const uint32_t nowMs = millis();
  readSerialInput();
  companionController.tick(nowMs);
  shtc3.tick(ambientModel, nowMs);
  audioDriver.tick();
  const bool keyChanged = handleKey(nowMs);
  if (keyChanged || nowMs - lastRenderMs >= 1000UL) {
    renderNow(nowMs);
  }
  delay(5);
}
