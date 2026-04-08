#pragma once
#include <stdint.h>

struct AtsData {
  bool source1SwitchClosed = false;
  bool source2SwitchClosed = false;
  bool source1VoltageNormal = false;
  bool source2VoltageNormal = false;
  bool autoMode = false;
  bool manualMode = false;
  bool startGeneratorOutput = false;
  bool commonWarning = false;
  bool commonAlarm = false;
  bool failToChangeover = false;
  bool source1OvervoltageAlarm = false;
  bool source1UndervoltageAlarm = false;
  bool source1OverfrequencyAlarm = false;
  bool source1UnderfrequencyAlarm = false;
  bool source2OvervoltageAlarm = false;
  bool source2UndervoltageAlarm = false;
  bool source2OverfrequencyAlarm = false;
  bool source2UnderfrequencyAlarm = false;

  float source1VoltageA = 0.0f;
  float source1VoltageB = 0.0f;
  float source1VoltageC = 0.0f;
  float source2VoltageA = 0.0f;
  float source2VoltageB = 0.0f;
  float source2VoltageC = 0.0f;
  float source1CurrentA = 0.0f;
  float source1CurrentB = 0.0f;
  float source1CurrentC = 0.0f;
  float source2CurrentA = 0.0f;
  float source2CurrentB = 0.0f;
  float source2CurrentC = 0.0f;
  float frequency1 = 0.0f;
  float frequency2 = 0.0f;
  float totalActivePowerKw = 0.0f;
  float totalApparentPowerKva = 0.0f;
  float totalPowerFactor = 0.0f;

  bool online = false;
};
