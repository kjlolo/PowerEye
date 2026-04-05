#pragma once
#include <stdint.h>

struct GensetData {
  float voltageA = 0.0f;
  float voltageB = 0.0f;
  float voltageC = 0.0f;
  float frequency = 0.0f;
  float currentA = 0.0f;
  float currentB = 0.0f;
  float currentC = 0.0f;
  float engineTemperature = 0.0f;
  float oilPressureKpa = 0.0f;
  float fuelLevelPercent = 0.0f;
  float speedRpm = 0.0f;
  float batteryVoltage = 0.0f;
  float chargerVoltage = 0.0f;
  float activePowerKw = 0.0f;
  float reactivePowerKvar = 0.0f;
  float apparentPowerKva = 0.0f;
  float powerFactor = 0.0f;
  uint16_t generatorState = 0;
  uint16_t mainsState = 0;
  uint32_t runHours = 0;
  uint32_t startCount = 0;
  uint32_t totalEnergy = 0;
  bool commonAlarm = false;
  bool commonWarn = false;
  bool commonShutdown = false;
  bool generatorNormal = false;
  bool mainsLoad = false;
  bool generatorLoad = false;
  bool autoMode = false;
  bool manualMode = false;
  bool stopMode = false;
  bool lowFuelWarn = false;
  bool chargeFailWarn = false;
  bool batteryUndervoltageWarn = false;
  bool batteryOvervoltageWarn = false;
  bool overSpeedShutdown = false;
  bool lowOilPressureShutdown = false;
  bool highEngineTemperatureShutdown = false;
  bool failedToStartShutdown = false;
  bool online = false;
};
