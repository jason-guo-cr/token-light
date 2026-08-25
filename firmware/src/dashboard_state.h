#pragma once

#include <Arduino.h>

struct LimitWindow {
  String label = "";
  int remainingPercent = 0;
  int usedPercent = 0;
  String resetLabel = "";
  bool valid = false;
};

struct DisplaySnapshot {
  String date = "--/--";
  String weekday = "---";
  String time = "--:--";
  String status = "boot";
  String message = "";
  int batteryPercent = -1;
  bool batteryCharging = false;
  String tokenTodayLabel = "";
  String tokenWeekLabel = "";
  String tokenBurnLabel = "0/H";
  String forecastLabel = "EST --";
  String paceLabel = "UNKNOWN";
  String limitUpdatedLabel = "";
  String weatherDisplay = "";
  String activityState = "idle";
  String activityLabel = "IDLE";
  String activityDetail = "NO ACTIVE TASK";
  unsigned long activityElapsedSeconds = 0;
  unsigned long completionSequence = 0;
  String petPose = "sleep";
  bool celebrationActive = false;
  unsigned long celebrationUntilMs = 0;
  bool audioEnabled = true;
  bool audioQuiet = false;
  LimitWindow primary;
  unsigned long receivedAtMs = 0;
};
