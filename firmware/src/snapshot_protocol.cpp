#include "snapshot_protocol.h"

#include <string.h>

namespace {
void updateString(JsonVariantConst value, std::string &target) {
  if (value.is<const char *>()) target = value.as<const char *>();
}

bool validPercent(JsonVariantConst value) {
  return value.is<int>() && value.as<int>() >= 0 && value.as<int>() <= 100;
}

void setActivityState(const char *value, SnapshotProtocolState &state) {
  const char *activity = value ? value : "idle";
  state.activityState = activity;
  if (state.activityState == "thinking") {
    state.activityLabel = "THINKING";
    state.activityDetail = "PLANNING";
  } else if (state.activityState == "reading") {
    state.activityLabel = "READING";
    state.activityDetail = "RESEARCH";
  } else if (state.activityState == "editing") {
    state.activityLabel = "EDITING";
    state.activityDetail = "CODE CHANGE";
  } else if (state.activityState == "testing") {
    state.activityLabel = "TESTING";
    state.activityDetail = "TEST RUN";
  } else if (state.activityState == "working") {
    state.activityLabel = "WORKING";
    state.activityDetail = "TOOL RUN";
  } else if (state.activityState == "waiting") {
    state.activityLabel = "WAITING";
    state.activityDetail = "NO RECENT EVENTS";
  } else if (state.activityState == "done") {
    state.activityLabel = "DONE";
    state.activityDetail = "TASK COMPLETE";
  } else if (state.activityState == "error") {
    state.activityLabel = "STOPPED";
    state.activityDetail = "TASK STOPPED";
  } else {
    state.activityState = "idle";
    state.activityLabel = "IDLE";
    state.activityDetail = "NO ACTIVE TASK";
  }
}

bool validPetPose(const char *pose) {
  if (!pose) return false;
  return strcmp(pose, "sleep") == 0 || strcmp(pose, "working") == 0 ||
         strcmp(pose, "coding") == 0 || strcmp(pose, "testing") == 0 ||
         strcmp(pose, "waiting") == 0 || strcmp(pose, "focus") == 0 ||
         strcmp(pose, "tired") == 0 || strcmp(pose, "celebrate") == 0 ||
         strcmp(pose, "alert") == 0;
}

void applyPrimary(JsonObjectConst object, ProtocolLimitWindow &primary) {
  if (object.isNull()) return;
  updateString(object["label"], primary.label);
  updateString(object["reset_label"], primary.resetLabel);
  if (validPercent(object["remaining_percent"])) {
    primary.remainingPercent = object["remaining_percent"].as<int>();
    primary.valid = true;
  }
  if (validPercent(object["used_percent"])) {
    primary.usedPercent = object["used_percent"].as<int>();
  }
}

void applyForecast(JsonObjectConst forecast, SnapshotProtocolState &state) {
  if (forecast.isNull()) return;
  const char *pace = forecast["pace"].is<const char *>()
                         ? forecast["pace"].as<const char *>()
                         : nullptr;
  if (pace) {
    if (strcmp(pace, "cool") == 0) state.paceLabel = "COOL";
    else if (strcmp(pace, "normal") == 0) state.paceLabel = "NORMAL";
    else if (strcmp(pace, "hot") == 0) state.paceLabel = "HOT";
    else if (strcmp(pace, "meltdown") == 0) state.paceLabel = "MELTDOWN";
    else state.paceLabel = "UNKNOWN";
  }
  if (validPercent(forecast["projected_used_percent"])) {
    state.forecastLabel =
        "EST " + std::to_string(forecast["projected_used_percent"].as<int>()) + "%";
  } else if (forecast["projected_used_percent"].isNull()) {
    state.forecastLabel = "EST --";
  }
}
}  // namespace

SnapshotProtocolResult applySnapshotProtocol(
    JsonDocument &doc, SnapshotProtocolState &state) {
  SnapshotProtocolResult result;
  const char *type = doc["type"] | "";
  if (strcmp(type, "snapshot") != 0) return result;
  result.accepted = true;

  updateString(doc["date"], state.date);
  updateString(doc["weekday"], state.weekday);
  updateString(doc["time"], state.time);
  updateString(doc["status"], state.status);
  updateString(doc["message"], state.message);
  updateString(doc["limit_updated_label"], state.limitUpdatedLabel);

  JsonObjectConst battery = doc["battery"].as<JsonObjectConst>();
  if (!battery.isNull()) {
    if (battery["percent"].is<int>()) state.batteryPercent = battery["percent"].as<int>();
    if (battery["charging"].is<bool>()) state.batteryCharging = battery["charging"].as<bool>();
  }
  JsonObjectConst tokenUsage = doc["token_usage"].as<JsonObjectConst>();
  if (!tokenUsage.isNull()) {
    updateString(tokenUsage["today_label"], state.tokenTodayLabel);
    updateString(tokenUsage["week_label"], state.tokenWeekLabel);
    updateString(tokenUsage["burn_label"], state.tokenBurnLabel);
  }
  JsonObjectConst weather = doc["weather"].as<JsonObjectConst>();
  if (!weather.isNull()) updateString(weather["display"], state.weatherDisplay);
  if (state.status == "live" || state.status == "cached") {
    applyPrimary(doc["primary"].as<JsonObjectConst>(), state.primary);
  }
  applyForecast(doc["forecast"].as<JsonObjectConst>(), state);

  JsonObjectConst companion = doc["companion"].as<JsonObjectConst>();
  if (!companion.isNull()) {
    JsonObjectConst activity = companion["activity"].as<JsonObjectConst>();
    if (!activity.isNull()) {
      if (activity["state"].is<const char *>()) {
        setActivityState(activity["state"].as<const char *>(), state);
      }
      if (activity["elapsed_seconds"].is<uint32_t>()) {
        state.activityElapsedSeconds = activity["elapsed_seconds"].as<uint32_t>();
      }
      if (activity["completion_seq"].is<uint32_t>()) {
        state.completionSequence = activity["completion_seq"].as<uint32_t>();
        result.hasCompletionSequence = true;
      }
    }
    JsonObjectConst pet = companion["pet"].as<JsonObjectConst>();
    const char *pose = pet["pose"].is<const char *>() ? pet["pose"].as<const char *>() : nullptr;
    if (validPetPose(pose)) state.petPose = pose;
  }
  JsonObjectConst audio = doc["audio"].as<JsonObjectConst>();
  if (!audio.isNull()) {
    if (audio["enabled"].is<bool>()) state.audioEnabled = audio["enabled"].as<bool>();
    if (audio["quiet"].is<bool>()) state.audioQuiet = audio["quiet"].as<bool>();
  }
  return result;
}
