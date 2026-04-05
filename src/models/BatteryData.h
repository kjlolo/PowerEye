#pragma once
#include <stdint.h>

struct BatteryData {
  static constexpr int MAX_CELL_VOLTAGES = 16;
  static constexpr int MAX_CELL_TEMPERATURES = 4;

  float packCurrent = 0.0f;
  float packVoltage = 0.0f;
  float soc = 0.0f;
  float soh = 0.0f;
  float remainingCapacityAh = 0.0f;
  float fullCapacityAh = 0.0f;
  float designCapacityAh = 0.0f;
  uint16_t cycleCount = 0;
  float mosfetTemperature = 0.0f;
  float environmentTemperature = 0.0f;
  float dischargeRemainingMinutes = 0.0f;
  float chargeRemainingMinutes = 0.0f;
  uint16_t warningFlags = 0;
  uint16_t protectionFlags = 0;
  uint16_t statusFlags = 0;
  uint16_t balanceStatus = 0;
  float cellVoltagesMv[MAX_CELL_VOLTAGES] = {};
  float cellTemperaturesC[MAX_CELL_TEMPERATURES] = {};
  bool lowSocAlarm = false;
  bool cellOvervoltageAlarm = false;
  bool cellUndervoltageAlarm = false;
  bool packOvervoltageAlarm = false;
  bool packUndervoltageAlarm = false;
  bool chargeOvercurrentAlarm = false;
  bool dischargeOvercurrentAlarm = false;
  bool chargeMosfetOn = false;
  bool dischargeMosfetOn = false;
  bool heaterOn = false;
  bool online = false;
};
