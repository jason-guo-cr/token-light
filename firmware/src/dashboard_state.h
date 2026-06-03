#pragma once

#include <Arduino.h>

struct LimitWindow {
  String label = "";
  int remainingPercent = 0;
  int usedPercent = 0;
  String resetLabel = "";
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
  String limitUpdatedLabel = "";
  String weatherDisplay = "";
  LimitWindow primary;
  LimitWindow secondary;
  unsigned long receivedAtMs = 0;
};
