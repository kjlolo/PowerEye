#pragma once
#include <Arduino.h>
#include "config/DeviceConfig.h"
#include "EnergyData.h"
#include "FuelData.h"
#include "AlarmState.h"
#include "GensetData.h"
#include "BatteryData.h"

struct TelemetrySnapshot {
  String deviceId;
  String siteId;
  String siteName;
  unsigned long uptimeMs = 0;
  size_t queuePending = 0;
  int rssi = 0;
  bool networkOnline = false;
  String phoneNumber;
  String fwVersion;
  String transportStatus;
  String lastError;
  bool cfgPzemEnabled = true;
  bool cfgGeneratorEnabled = false;
  bool cfgBatteryEnabled = false;
  bool cfgFuelEnabled = true;

  EnergyData energy;
  uint8_t gensetCountConfigured = 0;
  uint8_t gensetSlaveIds[Rs485Config::MAX_GENERATORS] = {};
  GeneratorModel gensetModels[Rs485Config::MAX_GENERATORS] = {
    GeneratorModel::NONE, GeneratorModel::NONE, GeneratorModel::NONE, GeneratorModel::NONE
  };
  GensetData gensets[Rs485Config::MAX_GENERATORS];
  uint8_t batteryBankCountConfigured = 0;
  uint8_t batteryBankSlaveIds[Rs485Config::MAX_BATTERY_BANKS] = {};
  BatteryModel batteryBankModels[Rs485Config::MAX_BATTERY_BANKS] = {
    BatteryModel::NONE, BatteryModel::NONE, BatteryModel::NONE, BatteryModel::NONE,
    BatteryModel::NONE, BatteryModel::NONE, BatteryModel::NONE, BatteryModel::NONE,
    BatteryModel::NONE, BatteryModel::NONE, BatteryModel::NONE, BatteryModel::NONE,
    BatteryModel::NONE, BatteryModel::NONE, BatteryModel::NONE, BatteryModel::NONE
  };
  BatteryData batteryBanks[Rs485Config::MAX_BATTERY_BANKS];
  FuelData fuel;
  AlarmState alarms;
};
