#include "app/TelemetryBuilder.h"
#include <ArduinoJson.h>

namespace {
uint8_t effectiveGensetCount(const TelemetrySnapshot& snapshot) {
  return snapshot.gensetCountConfigured > Rs485Config::MAX_GENERATORS
    ? Rs485Config::MAX_GENERATORS
    : snapshot.gensetCountConfigured;
}

uint8_t effectiveBatteryCount(const TelemetrySnapshot& snapshot) {
  return snapshot.batteryBankCountConfigured > Rs485Config::MAX_BATTERY_BANKS
    ? Rs485Config::MAX_BATTERY_BANKS
    : snapshot.batteryBankCountConfigured;
}

bool isGensetRunning(const GensetData& g) {
  if (!g.online) return false;
  if (g.activePowerKw > 0.2f || g.speedRpm > 200.0f) return true;
  if (g.generatorLoad || g.generatorState != 0) return true;
  return false;
}

int firstConfiguredOrOnlineGenset(const TelemetrySnapshot& snapshot, uint8_t gensetCount) {
  int firstConfigured = -1;
  for (uint8_t i = 0; i < gensetCount; ++i) {
    if (snapshot.gensetModels[i] == GeneratorModel::NONE) {
      continue;
    }
    if (firstConfigured < 0) {
      firstConfigured = i;
    }
    if (snapshot.gensets[i].online) {
      return i;
    }
  }
  return firstConfigured;
}

int firstDischargingBattery(const TelemetrySnapshot& snapshot, uint8_t batteryCount) {
  for (uint8_t i = 0; i < batteryCount; ++i) {
    const BatteryData& b = snapshot.batteryBanks[i];
    if (b.online && b.packCurrent < -0.2f) {
      return i;
    }
  }
  return -1;
}

String buildPowerEyePayload(const TelemetrySnapshot& snapshot, bool completePayload) {
  JsonDocument doc;
  doc["device_id"] = snapshot.deviceId;
  doc["fw_version"] = snapshot.fwVersion;
  doc["site_id"] = snapshot.siteId;
  doc["site_name"] = snapshot.siteName;
  doc["uptime_ms"] = snapshot.uptimeMs;
  doc["sample_ts_ms"] = snapshot.uptimeMs;
  doc["queue_pending"] = snapshot.queuePending;
  doc["rssi"] = snapshot.rssi;
  doc["phone_number"] = snapshot.phoneNumber;
  doc["network_online"] = snapshot.networkOnline;
  doc["transport_status"] = snapshot.transportStatus;
  doc["last_error"] = snapshot.lastError;
  doc["cfg_pzem_enabled"] = snapshot.cfgPzemEnabled;
  doc["cfg_ats_enabled"] = snapshot.cfgAtsEnabled;
  doc["cfg_generator_enabled"] = snapshot.cfgGeneratorEnabled;
  doc["cfg_battery_enabled"] = snapshot.cfgBatteryEnabled;
  doc["cfg_fuel_enabled"] = snapshot.cfgFuelEnabled;
  doc["fuel_sensor_online"] = snapshot.fuel.online;
  doc["fuel_sensor_status"] = snapshot.fuel.online ? "online" : "offline";

  const bool gridAvailable = snapshot.energy.online && snapshot.energy.voltage > 150.0f;
  const uint8_t gensetCount = effectiveGensetCount(snapshot);
  const uint8_t batteryCount = effectiveBatteryCount(snapshot);
  doc["genset_count_configured"] = gensetCount;
  doc["battery_bank_count_configured"] = batteryCount;

  bool gensetAvailable = false;
  bool gensetAnyAlarm = false;
  uint8_t gensetOnlineCount = 0;
  for (uint8_t i = 0; i < gensetCount; ++i) {
    const GensetData& g = snapshot.gensets[i];
    if (g.online) {
      ++gensetOnlineCount;
      if (isGensetRunning(g)) {
        gensetAvailable = true;
      }
    }
    if (g.commonAlarm || g.commonWarn || g.commonShutdown) {
      gensetAnyAlarm = true;
    }
  }

  bool batteryAvailable = false;
  bool batteryDischarging = false;
  uint8_t batteryOnlineCount = 0;
  uint8_t batteryLowSocCount = 0;
  uint8_t batteryDischargingCount = 0;
  uint8_t batteryChargingCount = 0;
  for (uint8_t i = 0; i < batteryCount; ++i) {
    const BatteryData& b = snapshot.batteryBanks[i];
    if (b.online) {
      ++batteryOnlineCount;
      batteryAvailable = true;
      if (b.packCurrent < -0.2f) {
        batteryDischarging = true;
        ++batteryDischargingCount;
      } else if (b.packCurrent > 0.2f) {
        ++batteryChargingCount;
      }
      if (b.soc <= 20.0f) {
        ++batteryLowSocCount;
      }
    }
  }

  const bool sitePowerAvailable = gridAvailable || gensetAvailable || batteryDischarging;
  const char* powerSource = "none";
  if (gridAvailable) {
    powerSource = "grid";
  } else if (gensetAvailable) {
    powerSource = "genset";
  } else if (batteryDischarging) {
    powerSource = "battery";
  }
  doc["site_power_available"] = sitePowerAvailable;
  doc["power_source"] = powerSource;
  doc["power_supply_grid"] = (strcmp(powerSource, "grid") == 0);
  doc["power_supply_genset"] = (strcmp(powerSource, "genset") == 0);
  doc["power_supply_battery"] = (strcmp(powerSource, "battery") == 0);
  doc["genset_online_count"] = gensetOnlineCount;
  doc["genset_any_alarm"] = gensetAnyAlarm;
  doc["battery_online_count"] = batteryOnlineCount;
  doc["battery_discharging_count"] = batteryDischargingCount;
  doc["battery_charging_count"] = batteryChargingCount;
  doc["battery_discharging_active"] = batteryDischarging;
  doc["battery_low_soc_count"] = batteryLowSocCount;

  JsonObject energy = doc["energy"].to<JsonObject>();
  energy["voltage"] = snapshot.energy.voltage;
  energy["current"] = snapshot.energy.current;
  energy["power"] = snapshot.energy.power;
  energy["energy_kwh"] = snapshot.energy.energyKwh;
  energy["frequency"] = snapshot.energy.frequency;
  energy["power_factor"] = snapshot.energy.powerFactor;
  energy["alarm_status"] = snapshot.energy.alarmStatus;
  energy["online"] = snapshot.energy.online;

  const char* atsSupplySource = "none";
  if (snapshot.ats.source1SwitchClosed && !snapshot.ats.source2SwitchClosed) atsSupplySource = "commercial";
  else if (snapshot.ats.source2SwitchClosed && !snapshot.ats.source1SwitchClosed) atsSupplySource = "genset";
  else if (snapshot.ats.source1SwitchClosed && snapshot.ats.source2SwitchClosed) atsSupplySource = "both";
  doc["ats_online"] = snapshot.ats.online;
  doc["ats_supply_source"] = atsSupplySource;

  JsonObject ats = doc["ats"].to<JsonObject>();
  ats["slave_id"] = snapshot.atsSlaveId;
  ats["model"] = atsModelToString(snapshot.atsModel);
  ats["online"] = snapshot.ats.online;
  ats["source1_switch_closed"] = snapshot.ats.source1SwitchClosed;
  ats["source2_switch_closed"] = snapshot.ats.source2SwitchClosed;
  ats["source1_voltage_normal"] = snapshot.ats.source1VoltageNormal;
  ats["source2_voltage_normal"] = snapshot.ats.source2VoltageNormal;
  ats["auto_mode"] = snapshot.ats.autoMode;
  ats["manual_mode"] = snapshot.ats.manualMode;
  ats["start_generator_output"] = snapshot.ats.startGeneratorOutput;
  ats["common_warning"] = snapshot.ats.commonWarning;
  ats["common_alarm"] = snapshot.ats.commonAlarm;
  ats["fail_to_changeover"] = snapshot.ats.failToChangeover;
  ats["source1_voltage_a"] = snapshot.ats.source1VoltageA;
  ats["source2_voltage_a"] = snapshot.ats.source2VoltageA;
  ats["source1_current_a"] = snapshot.ats.source1CurrentA;
  ats["source2_current_a"] = snapshot.ats.source2CurrentA;
  ats["frequency1"] = snapshot.ats.frequency1;
  ats["frequency2"] = snapshot.ats.frequency2;
  ats["total_active_power_kw"] = snapshot.ats.totalActivePowerKw;
  ats["total_apparent_power_kva"] = snapshot.ats.totalApparentPowerKva;
  ats["total_power_factor"] = snapshot.ats.totalPowerFactor;
  if (completePayload) {
    ats["source1_voltage_b"] = snapshot.ats.source1VoltageB;
    ats["source1_voltage_c"] = snapshot.ats.source1VoltageC;
    ats["source2_voltage_b"] = snapshot.ats.source2VoltageB;
    ats["source2_voltage_c"] = snapshot.ats.source2VoltageC;
    ats["source1_current_b"] = snapshot.ats.source1CurrentB;
    ats["source1_current_c"] = snapshot.ats.source1CurrentC;
    ats["source2_current_b"] = snapshot.ats.source2CurrentB;
    ats["source2_current_c"] = snapshot.ats.source2CurrentC;
    ats["source1_overvoltage_alarm"] = snapshot.ats.source1OvervoltageAlarm;
    ats["source1_undervoltage_alarm"] = snapshot.ats.source1UndervoltageAlarm;
    ats["source1_overfrequency_alarm"] = snapshot.ats.source1OverfrequencyAlarm;
    ats["source1_underfrequency_alarm"] = snapshot.ats.source1UnderfrequencyAlarm;
    ats["source2_overvoltage_alarm"] = snapshot.ats.source2OvervoltageAlarm;
    ats["source2_undervoltage_alarm"] = snapshot.ats.source2UndervoltageAlarm;
    ats["source2_overfrequency_alarm"] = snapshot.ats.source2OverfrequencyAlarm;
    ats["source2_underfrequency_alarm"] = snapshot.ats.source2UnderfrequencyAlarm;
  }

  JsonArray gensets = doc["gensets"].to<JsonArray>();
  for (uint8_t i = 0; i < gensetCount; ++i) {
    const GensetData& g = snapshot.gensets[i];
    JsonObject gen = gensets.add<JsonObject>();
    gen["index"] = i + 1;
    gen["slave_id"] = snapshot.gensetSlaveIds[i];
    gen["model"] = generatorModelToString(snapshot.gensetModels[i]);
    gen["voltage_a"] = g.voltageA;
    gen["current_a"] = g.currentA;
    gen["frequency"] = g.frequency;
    gen["battery_voltage"] = g.batteryVoltage;
    gen["active_power_kw"] = g.activePowerKw;
    gen["run_hours"] = g.runHours;
    if (g.autoMode) gen["mode"] = "auto";
    else if (g.manualMode) gen["mode"] = "manual";
    else if (g.stopMode) gen["mode"] = "stop";
    else if (g.online) gen["mode"] = "test";
    else gen["mode"] = "unknown";
    gen["alarm"] = g.commonAlarm || g.commonWarn || g.commonShutdown;
    if (completePayload) {
      gen["voltage_b"] = g.voltageB;
      gen["voltage_c"] = g.voltageC;
      gen["current_b"] = g.currentB;
      gen["current_c"] = g.currentC;
      gen["fuel_level_percent"] = g.fuelLevelPercent;
      gen["speed_rpm"] = g.speedRpm;
      gen["engine_temp_c"] = g.engineTemperature;
      gen["oil_pressure_kpa"] = g.oilPressureKpa;
      gen["charger_voltage"] = g.chargerVoltage;
      gen["reactive_power_kvar"] = g.reactivePowerKvar;
      gen["apparent_power_kva"] = g.apparentPowerKva;
      gen["power_factor"] = g.powerFactor;
      gen["generator_state"] = g.generatorState;
      gen["mains_state"] = g.mainsState;
      gen["start_count"] = g.startCount;
      gen["total_energy"] = g.totalEnergy;
      gen["common_alarm"] = g.commonAlarm;
      gen["common_warn"] = g.commonWarn;
      gen["common_shutdown"] = g.commonShutdown;
      gen["generator_normal"] = g.generatorNormal;
      gen["mains_load"] = g.mainsLoad;
      gen["generator_load"] = g.generatorLoad;
      gen["auto_mode"] = g.autoMode;
      gen["manual_mode"] = g.manualMode;
      gen["stop_mode"] = g.stopMode;
      gen["low_fuel_warn"] = g.lowFuelWarn;
      gen["charge_fail_warn"] = g.chargeFailWarn;
      gen["battery_undervoltage_warn"] = g.batteryUndervoltageWarn;
      gen["battery_overvoltage_warn"] = g.batteryOvervoltageWarn;
      gen["overspeed_shutdown"] = g.overSpeedShutdown;
      gen["low_oil_pressure_shutdown"] = g.lowOilPressureShutdown;
      gen["high_engine_temperature_shutdown"] = g.highEngineTemperatureShutdown;
      gen["failed_to_start_shutdown"] = g.failedToStartShutdown;
    }
    gen["online"] = g.online;
  }

  JsonArray batteryBanks = doc["battery_banks"].to<JsonArray>();
  for (uint8_t i = 0; i < batteryCount; ++i) {
    const BatteryData& b = snapshot.batteryBanks[i];
    JsonObject bank = batteryBanks.add<JsonObject>();
    bank["index"] = i + 1;
    bank["slave_id"] = snapshot.batteryBankSlaveIds[i];
    bank["model"] = batteryModelToString(snapshot.batteryBankModels[i]);
    bank["pack_voltage"] = b.packVoltage;
    bank["pack_current"] = b.packCurrent;
    bank["soc"] = b.soc;
    bank["soh"] = b.soh;
    bank["alarm"] = b.warningFlags != 0 || b.protectionFlags != 0 || b.lowSocAlarm;
    if (completePayload) {
      bank["remaining_capacity_ah"] = b.remainingCapacityAh;
      bank["full_capacity_ah"] = b.fullCapacityAh;
      bank["design_capacity_ah"] = b.designCapacityAh;
      bank["cycle_count"] = b.cycleCount;
      bank["mosfet_temp_c"] = b.mosfetTemperature;
      bank["environment_temp_c"] = b.environmentTemperature;
      bank["discharge_remaining_minutes"] = b.dischargeRemainingMinutes;
      bank["charge_remaining_minutes"] = b.chargeRemainingMinutes;
      bank["warning_flags"] = b.warningFlags;
      bank["protection_flags"] = b.protectionFlags;
      bank["status_flags"] = b.statusFlags;
      bank["balance_status"] = b.balanceStatus;
      bank["low_soc_alarm"] = b.lowSocAlarm;
      bank["cell_overvoltage_alarm"] = b.cellOvervoltageAlarm;
      bank["cell_undervoltage_alarm"] = b.cellUndervoltageAlarm;
      bank["pack_overvoltage_alarm"] = b.packOvervoltageAlarm;
      bank["pack_undervoltage_alarm"] = b.packUndervoltageAlarm;
      bank["charge_overcurrent_alarm"] = b.chargeOvercurrentAlarm;
      bank["discharge_overcurrent_alarm"] = b.dischargeOvercurrentAlarm;
      bank["charge_mosfet_on"] = b.chargeMosfetOn;
      bank["discharge_mosfet_on"] = b.dischargeMosfetOn;
      bank["heater_on"] = b.heaterOn;
      JsonArray cellVoltages = bank["cell_voltages_mv"].to<JsonArray>();
      for (int c = 0; c < BatteryData::MAX_CELL_VOLTAGES; ++c) {
        cellVoltages.add(b.cellVoltagesMv[c]);
      }
      JsonArray cellTemps = bank["cell_temperatures_c"].to<JsonArray>();
      for (int t = 0; t < BatteryData::MAX_CELL_TEMPERATURES; ++t) {
        cellTemps.add(b.cellTemperaturesC[t]);
      }
    }
    bank["online"] = b.online;
  }

  JsonObject fuel = doc["fuel"].to<JsonObject>();
  fuel["raw"] = snapshot.fuel.raw;
  fuel["percent"] = snapshot.fuel.percent;
  fuel["liters"] = snapshot.fuel.liters;
  fuel["online"] = snapshot.fuel.online;

  JsonObject alarms = doc["alarms"].to<JsonObject>();
  alarms["ac_mains"] = snapshot.alarms.acMains;
  alarms["genset_run"] = snapshot.alarms.gensetRun;
  alarms["genset_fail"] = snapshot.alarms.gensetFail;
  alarms["battery_theft"] = snapshot.alarms.batteryTheft;
  alarms["power_cable_theft"] = snapshot.alarms.powerCableTheft;
  alarms["door_open"] = snapshot.alarms.doorOpen;

  String out;
  serializeJson(doc, out);
  return out;
}

String buildMcbeamPayload(const TelemetrySnapshot& snapshot, bool syncOnly, const String& syncRequestId) {
  JsonDocument doc;
  const uint8_t gensetCount = effectiveGensetCount(snapshot);
  const uint8_t batteryCount = effectiveBatteryCount(snapshot);
  const bool gridAvailable = snapshot.energy.online && snapshot.energy.voltage > 150.0f;

  bool gensetAvailable = false;
  for (uint8_t i = 0; i < gensetCount; ++i) {
    if (isGensetRunning(snapshot.gensets[i])) {
      gensetAvailable = true;
      break;
    }
  }

  const int dischargingBatteryIdx = firstDischargingBattery(snapshot, batteryCount);
  const bool batteryDischarging = dischargingBatteryIdx >= 0;

  int loadSource = 0;  // commercial
  const char* pzemSource = "commercial";
  if (gridAvailable) {
    loadSource = 0;
    pzemSource = "commercial";
  } else if (gensetAvailable) {
    loadSource = 2;
    pzemSource = "generator";
  } else if (batteryDischarging) {
    loadSource = 1;
    pzemSource = "unknown";
  } else {
    loadSource = 0;
    pzemSource = "unknown";
  }

  float activeVoltage = 0.0f;
  float activeCurrent = 0.0f;
  float activePower = 0.0f;
  float activeFrequency = 0.0f;
  float activePowerFactor = 0.0f;

  if (loadSource == 0 || loadSource == 2) {
    activeVoltage = snapshot.energy.voltage;
    activeCurrent = snapshot.energy.current;
    activePower = snapshot.energy.power;
    activeFrequency = snapshot.energy.frequency;
    activePowerFactor = snapshot.energy.powerFactor;
  } else if (loadSource == 1) {
    const BatteryData& b = snapshot.batteryBanks[dischargingBatteryIdx];
    activeVoltage = b.packVoltage;
    activeCurrent = b.packCurrent;
    activePower = b.packVoltage * (b.packCurrent < 0 ? -b.packCurrent : b.packCurrent);
    activeFrequency = 0.0f;
    activePowerFactor = 0.0f;
  }

  const int primaryGensetIdx = firstConfiguredOrOnlineGenset(snapshot, gensetCount);
  const GensetData* primaryGenset = primaryGensetIdx >= 0 ? &snapshot.gensets[primaryGensetIdx] : nullptr;
  const GeneratorModel primaryGensetModel = primaryGensetIdx >= 0
    ? snapshot.gensetModels[primaryGensetIdx]
    : GeneratorModel::NONE;

  const char* gensetType = "UNKNOWN";
  if (primaryGensetModel == GeneratorModel::HGM6100NC) {
    gensetType = "HGM6100NC";
  } else if (primaryGensetModel == GeneratorModel::HAT600) {
    gensetType = "HAT600";
  }

  const char* gensetMode = "UNKNOWN";
  const char* gensetStatus = "STOPPED";
  bool gensetOnLoad = false;
  bool gensetOk = false;
  float gensetVoltage = 0.0f;
  float gensetCurrent = 0.0f;
  float gensetPower = 0.0f;
  float gensetFrequency = 0.0f;
  float gensetPowerFactor = 0.0f;
  float gensetBatteryVoltage = 0.0f;
  float gensetFuelPercent = -1.0f;
  uint32_t gensetHours = 0;
  uint32_t gensetEnergy = 0;

  if (primaryGenset != nullptr) {
    const GensetData& g = *primaryGenset;
    if (g.autoMode) gensetMode = "AUTO";
    else if (g.manualMode) gensetMode = "MANUAL";
    else if (g.stopMode) gensetMode = "STOP";

    gensetStatus = isGensetRunning(g) ? "RUNNING" : "STOPPED";
    gensetOnLoad = g.generatorLoad;
    gensetOk = g.online;
    gensetVoltage = g.voltageA;
    gensetCurrent = g.currentA;
    gensetPower = g.activePowerKw;
    gensetFrequency = g.frequency;
    gensetPowerFactor = g.powerFactor;
    gensetBatteryVoltage = g.batteryVoltage;
    gensetFuelPercent = g.fuelLevelPercent;
    gensetHours = g.runHours;
    gensetEnergy = g.totalEnergy;
  }

  float fuelTankCapacity = 0.0f;
  if (snapshot.fuel.online && snapshot.fuel.percent > 0.01f) {
    fuelTankCapacity = snapshot.fuel.liters * 100.0f / snapshot.fuel.percent;
  }

  doc["site_id"] = snapshot.siteId;
  doc["fw"] = snapshot.fwVersion;
  doc["device_mac"] = snapshot.deviceMac;
  doc["rssi"] = snapshot.rssi;
  doc["phone_number"] = snapshot.phoneNumber;
  doc["group"] = snapshot.group;
  doc["province"] = snapshot.province;
  doc["city"] = snapshot.city;
  doc["sequence_id"] = snapshot.sequenceId;

  doc["temperature"] = 0.0f;
  doc["humidity"] = 0.0f;
  doc["dht_ok"] = false;
  doc["dht_temp_valid"] = false;
  doc["dht_hum_valid"] = false;

  doc["commercial_voltage"] = snapshot.energy.voltage;
  doc["commercial_current"] = snapshot.energy.current;
  doc["commercial_power"] = snapshot.energy.power;
  doc["commercial_energy"] = snapshot.energy.energyKwh;
  doc["commercial_frequency"] = snapshot.energy.frequency;
  doc["pzem_ok"] = snapshot.energy.online;
  doc["pzem_voltage_valid"] = snapshot.energy.online;
  doc["pzem_current_valid"] = snapshot.energy.online;
  doc["pzem_power_valid"] = snapshot.energy.online;
  doc["pzem_energy_valid"] = snapshot.energy.online;
  doc["pzem_frequency_valid"] = snapshot.energy.online;
  doc["pzem_read_ms"] = snapshot.uptimeMs;
  doc["power_factor"] = snapshot.energy.powerFactor;
  doc["pzem_source"] = pzemSource;
  doc["ats_online"] = snapshot.ats.online;
  doc["ats_source1_switch_closed"] = snapshot.ats.source1SwitchClosed;
  doc["ats_source2_switch_closed"] = snapshot.ats.source2SwitchClosed;

  doc["load_source"] = loadSource;
  doc["active_voltage"] = activeVoltage;
  doc["active_current"] = activeCurrent;
  doc["active_power"] = activePower;
  doc["active_frequency"] = activeFrequency;
  doc["active_power_factor"] = activePowerFactor;

  doc["genset_type"] = gensetType;
  doc["genset_status"] = gensetStatus;
  doc["genset_mode"] = gensetMode;
  doc["genset_voltage"] = gensetVoltage;
  doc["genset_current"] = gensetCurrent;
  doc["genset_power"] = gensetPower;
  doc["genset_frequency"] = gensetFrequency;
  doc["genset_energy"] = gensetEnergy;
  doc["genset_battery_voltage"] = gensetBatteryVoltage;
  doc["genset_hours"] = gensetHours;
  doc["genset_fuel_percent"] = gensetFuelPercent;
  doc["genset_power_factor"] = gensetPowerFactor;
  doc["genset_ok"] = gensetOk;
  doc["genset_breaker"] = gensetOnLoad ? "CLOSED" : "OPEN";

  JsonArray coils = doc["genset_coils"].to<JsonArray>();
  if (primaryGenset != nullptr) {
    const GensetData& g = *primaryGenset;
    coils.add(g.generatorLoad ? 1 : 0);
    coils.add(g.mainsLoad ? 1 : 0);
    coils.add(g.autoMode ? 1 : 0);
    coils.add(g.manualMode ? 1 : 0);
    coils.add(g.stopMode ? 1 : 0);
    coils.add(g.commonAlarm ? 1 : 0);
    coils.add(g.commonWarn ? 1 : 0);
    coils.add(g.commonShutdown ? 1 : 0);
    coils.add(g.lowFuelWarn ? 1 : 0);
    coils.add(g.chargeFailWarn ? 1 : 0);
    coils.add(g.batteryUndervoltageWarn ? 1 : 0);
    coils.add(g.batteryOvervoltageWarn ? 1 : 0);
    coils.add(g.overSpeedShutdown ? 1 : 0);
    coils.add(g.lowOilPressureShutdown ? 1 : 0);
    coils.add(g.highEngineTemperatureShutdown ? 1 : 0);
    coils.add(g.failedToStartShutdown ? 1 : 0);
  }

  doc["fuel"] = snapshot.fuel.percent;
  doc["fuel_voltage"] = (snapshot.fuel.raw / 4095.0f) * 3.3f;
  doc["fuel_tank_capacity"] = fuelTankCapacity;
  doc["fuel_liters"] = snapshot.fuel.liters;

  JsonArray rectifiers = doc["rectifiers"].to<JsonArray>();
  JsonObject rect = rectifiers.add<JsonObject>();
  rect["id"] = 1;
  JsonArray batteries = rect["batteries"].to<JsonArray>();
  for (uint8_t idx = 0; idx < batteryCount; ++idx) {
    const BatteryData& b = snapshot.batteryBanks[idx];
    if (!b.online) continue;
    JsonObject batt = batteries.add<JsonObject>();
    batt["id"] = snapshot.batteryBankSlaveIds[idx];
    batt["voltage"] = b.packVoltage;
    batt["current"] = b.packCurrent;
    batt["soc"] = b.soc;
    batt["soh"] = b.soh;
  }

  if (syncOnly && syncRequestId.length() > 0) {
    doc["sync_request_id"] = syncRequestId;
    doc["sync_only"] = true;
  }

  String out;
  serializeJson(doc, out);
  return out;
}
}

String TelemetryBuilder::buildJson(const TelemetrySnapshot& snapshot,
                                   bool completePayload,
                                   bool mcbeamCompatPayload,
                                   bool syncOnly,
                                   const String& syncRequestId) const {
  if (mcbeamCompatPayload) {
    return buildMcbeamPayload(snapshot, syncOnly, syncRequestId);
  }
  return buildPowerEyePayload(snapshot, completePayload);
}
