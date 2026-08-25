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
  snapshot.tokenTodayLabel = "0";
  snapshot.tokenWeekLabel = "0";
  snapshot.limitUpdatedLabel = "";
  snapshot.weatherDisplay = "";
  snapshot.audioQuiet = true;
  setWindow(snapshot.primary, "CODEX WEEK", 0, 0, "--/--");
  snapshot.receivedAtMs = 0;
}

static void updateString(JsonVariantConst value, String &target) {
  if (value.is<const char *>()) {
    target = value.as<const char *>();
  }
}

static void updateWindow(JsonObjectConst obj, LimitWindow &window) {
  if (obj.isNull()) return;
  updateString(obj["label"], window.label);
  updateString(obj["reset_label"], window.resetLabel);
  if (obj["remaining_percent"].is<int>()) window.remainingPercent = obj["remaining_percent"].as<int>();
  if (obj["used_percent"].is<int>()) window.usedPercent = obj["used_percent"].as<int>();
}

static bool setActivityState(const char *state) {
  if (!state) return false;
  snapshot.activityState = state;
  if (snapshot.activityState == "idle") {
    snapshot.activityLabel = "IDLE";
    snapshot.activityDetail = "NO ACTIVE TASK";
  } else if (snapshot.activityState == "thinking") {
    snapshot.activityLabel = "THINKING";
    snapshot.activityDetail = "PLANNING";
  } else if (snapshot.activityState == "reading") {
    snapshot.activityLabel = "READING";
    snapshot.activityDetail = "RESEARCH";
  } else if (snapshot.activityState == "editing") {
    snapshot.activityLabel = "EDITING";
    snapshot.activityDetail = "CODE CHANGE";
  } else if (snapshot.activityState == "testing") {
    snapshot.activityLabel = "TESTING";
    snapshot.activityDetail = "TEST RUN";
  } else if (snapshot.activityState == "working") {
    snapshot.activityLabel = "WORKING";
    snapshot.activityDetail = "TOOL RUN";
  } else if (snapshot.activityState == "waiting") {
    snapshot.activityLabel = "WAITING";
    snapshot.activityDetail = "NO RECENT EVENTS";
  } else if (snapshot.activityState == "done") {
    snapshot.activityLabel = "DONE";
    snapshot.activityDetail = "TASK COMPLETE";
  } else if (snapshot.activityState == "error") {
    snapshot.activityLabel = "STOPPED";
    snapshot.activityDetail = "TASK STOPPED";
  } else {
    snapshot.activityState = "idle";
    snapshot.activityLabel = "IDLE";
    snapshot.activityDetail = "NO ACTIVE TASK";
  }
  return true;
}

static bool validPetPose(const char *pose) {
  if (!pose) return false;
  return strcmp(pose, "sleep") == 0 || strcmp(pose, "working") == 0 || strcmp(pose, "coding") == 0 ||
         strcmp(pose, "testing") == 0 || strcmp(pose, "waiting") == 0 || strcmp(pose, "focus") == 0 ||
         strcmp(pose, "tired") == 0 || strcmp(pose, "celebrate") == 0 || strcmp(pose, "alert") == 0;
}

static void applyForecast(JsonObjectConst forecast) {
  if (forecast.isNull()) return;
  const char *pace = forecast["pace"] | nullptr;
  if (pace) {
    if (strcmp(pace, "cool") == 0) snapshot.paceLabel = "COOL";
    else if (strcmp(pace, "normal") == 0) snapshot.paceLabel = "NORMAL";
    else if (strcmp(pace, "hot") == 0) snapshot.paceLabel = "HOT";
    else if (strcmp(pace, "meltdown") == 0) snapshot.paceLabel = "MELTDOWN";
    else snapshot.paceLabel = "UNKNOWN";
  }
  if (forecast["projected_used_percent"].is<int>()) {
    const int projected = constrain(forecast["projected_used_percent"].as<int>(), 0, 100);
    snapshot.forecastLabel = "EST " + String(projected) + "%";
  } else if (forecast["projected_used_percent"].isNull()) {
    snapshot.forecastLabel = "EST --";
  }
}

static int snapshotHour() {
  if (snapshot.time.length() < 2 || !isDigit(snapshot.time[0]) || !isDigit(snapshot.time[1])) return 12;
  return (snapshot.time[0] - '0') * 10 + (snapshot.time[1] - '0');
}

static bool applySnapshot(JsonDocument &doc) {
  const char *type = doc["type"] | "";
  if (strcmp(type, "snapshot") != 0) {
    Serial.println("token-light: ignored non-snapshot JSON");
    return false;
  }

  updateString(doc["date"], snapshot.date);
  updateString(doc["weekday"], snapshot.weekday);
  updateString(doc["time"], snapshot.time);
  updateString(doc["status"], snapshot.status);
  updateString(doc["message"], snapshot.message);
  updateString(doc["limit_updated_label"], snapshot.limitUpdatedLabel);

  JsonObjectConst battery = doc["battery"].as<JsonObjectConst>();
  if (!battery.isNull()) {
    if (battery["percent"].is<int>()) snapshot.batteryPercent = battery["percent"].as<int>();
    if (battery["charging"].is<bool>()) snapshot.batteryCharging = battery["charging"].as<bool>();
  }
  JsonObjectConst tokenUsage = doc["token_usage"].as<JsonObjectConst>();
  if (!tokenUsage.isNull()) {
    updateString(tokenUsage["today_label"], snapshot.tokenTodayLabel);
    updateString(tokenUsage["week_label"], snapshot.tokenWeekLabel);
    updateString(tokenUsage["burn_label"], snapshot.tokenBurnLabel);
  }
  JsonObjectConst weather = doc["weather"].as<JsonObjectConst>();
  if (!weather.isNull()) updateString(weather["display"], snapshot.weatherDisplay);
  if (snapshot.status == "live" || snapshot.status == "cached") {
    updateWindow(doc["primary"].as<JsonObjectConst>(), snapshot.primary);
  }
  applyForecast(doc["forecast"].as<JsonObjectConst>());

  bool hasCompletionSequence = false;
  JsonObjectConst companion = doc["companion"].as<JsonObjectConst>();
  if (!companion.isNull()) {
    JsonObjectConst activity = companion["activity"].as<JsonObjectConst>();
    if (!activity.isNull()) {
      if (activity["state"].is<const char *>()) setActivityState(activity["state"].as<const char *>());
      if (activity["elapsed_seconds"].is<unsigned long>()) {
        snapshot.activityElapsedSeconds = activity["elapsed_seconds"].as<unsigned long>();
      }
      if (activity["completion_seq"].is<unsigned long>()) {
        snapshot.completionSequence = activity["completion_seq"].as<unsigned long>();
        hasCompletionSequence = true;
      }
    }
    JsonObjectConst pet = companion["pet"].as<JsonObjectConst>();
    const char *pose = pet["pose"] | nullptr;
    if (validPetPose(pose)) snapshot.petPose = pose;
  }
  JsonObjectConst audio = doc["audio"].as<JsonObjectConst>();
  if (!audio.isNull()) {
    if (audio["enabled"].is<bool>()) snapshot.audioEnabled = audio["enabled"].as<bool>();
    if (audio["quiet"].is<bool>()) snapshot.audioQuiet = audio["quiet"].as<bool>();
  }

  snapshot.receivedAtMs = millis();
  return hasCompletionSequence;
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
    const int hour = snapshotHour();
    const bool quietHour = hour >= 22 || hour < 8;
    const bool audioAlways = quietHour && !snapshot.audioQuiet;
    if (completionNotifier.shouldPlay(snapshot.completionSequence, hour, snapshot.audioEnabled, audioAlways)) {
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
