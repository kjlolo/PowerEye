#include "app/TelemetryBuilder.h"
#include <ArduinoJson.h>

String TelemetryBuilder::buildJson(const TelemetrySnapshot& snapshot, bool completePayload) const {
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
  doc["fuel_sensor_online"] = snapshot.fuel.online;
  doc["fuel_sensor_status"] = snapshot.fuel.online ? "online" : "offline";

  const bool gridAvailable = snapshot.energy.online && snapshot.energy.voltage > 150.0f;
  const uint8_t gensetCount = snapshot.gensetCountConfigured > Rs485Config::MAX_GENERATORS
    ? Rs485Config::MAX_GENERATORS
    : snapshot.gensetCountConfigured;
  const uint8_t batteryCount = snapshot.batteryBankCountConfigured > Rs485Config::MAX_BATTERY_BANKS
    ? Rs485Config::MAX_BATTERY_BANKS
    : snapshot.batteryBankCountConfigured;
  doc["genset_count_configured"] = gensetCount;
  doc["battery_bank_count_configured"] = batteryCount;

  bool gensetAvailable = false;
  bool gensetAnyAlarm = false;
  uint8_t gensetOnlineCount = 0;
  for (uint8_t i = 0; i < gensetCount; ++i) {
    const GensetData& g = snapshot.gensets[i];
    if (g.online) {
      ++gensetOnlineCount;
      if (g.activePowerKw > 0.2f || g.speedRpm > 200.0f) {
        gensetAvailable = true;
      }
    }
    if (g.commonAlarm || g.commonWarn || g.commonShutdown) {
      gensetAnyAlarm = true;
    }
  }

  bool batteryAvailable = false;
  uint8_t batteryOnlineCount = 0;
  uint8_t batteryLowSocCount = 0;
  for (uint8_t i = 0; i < batteryCount; ++i) {
    const BatteryData& b = snapshot.batteryBanks[i];
    if (b.online) {
      ++batteryOnlineCount;
      batteryAvailable = true;
      if (b.soc <= 20.0f) {
        ++batteryLowSocCount;
      }
    }
  }

  const bool sitePowerAvailable = gridAvailable || gensetAvailable || batteryAvailable;
  const char* powerSource = "none";
  if (gridAvailable) {
    powerSource = "grid";
  } else if (gensetAvailable) {
    powerSource = "genset";
  } else if (batteryAvailable) {
    powerSource = "battery";
  }
  doc["site_power_available"] = sitePowerAvailable;
  doc["power_source"] = powerSource;
  doc["genset_online_count"] = gensetOnlineCount;
  doc["genset_any_alarm"] = gensetAnyAlarm;
  doc["battery_online_count"] = batteryOnlineCount;
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
