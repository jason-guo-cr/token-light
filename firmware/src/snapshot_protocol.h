#pragma once

#include <ArduinoJson.h>
#include <stdint.h>
#include <string>

struct ProtocolLimitWindow {
  std::string label = "CODEX WEEK";
  int remainingPercent = 0;
  int usedPercent = 0;
  std::string resetLabel = "--/--";
  bool valid = false;
};

struct SnapshotProtocolState {
  std::string date = "BOOT";
  std::string weekday = "DBG";
  std::string time = "00:00";
  std::string status = "boot";
  std::string message = "WAITING FOR HOST";
  int batteryPercent = -1;
  bool batteryCharging = false;
  std::string tokenTodayLabel = "0";
  std::string tokenWeekLabel = "0";
  std::string tokenBurnLabel = "0/H";
  std::string forecastLabel = "EST --";
  std::string paceLabel = "UNKNOWN";
  std::string limitUpdatedLabel;
  std::string weatherDisplay;
  std::string activityState = "idle";
  std::string activityLabel = "IDLE";
  std::string activityDetail = "NO ACTIVE TASK";
  uint32_t activityElapsedSeconds = 0;
  uint32_t completionSequence = 0;
  std::string petPose = "sleep";
  bool audioEnabled = true;
  bool audioQuiet = true;
  ProtocolLimitWindow primary;
};

struct SnapshotProtocolResult {
  bool accepted = false;
  bool hasCompletionSequence = false;
};

SnapshotProtocolResult applySnapshotProtocol(
    JsonDocument &doc, SnapshotProtocolState &state);
