#pragma once

struct AlarmState {
  bool acMains = false;
  bool gensetRun = false;
  bool gensetFail = false;
  bool batteryTheft = false;
  bool powerCableTheft = false;
  bool doorOpen = false;
};
