#include "storage/PreferencesStore.h"

bool PreferencesStore::begin() {
  return _prefs.begin("powereye", false);
}

bool PreferencesStore::load(DeviceConfig& config) {
  config.identity.deviceId = _prefs.getString("device_id", config.identity.deviceId);
  config.identity.siteId = _prefs.getString("site_id", config.identity.siteId);
  config.identity.siteName = _prefs.getString("site_name", config.identity.siteName);

  config.cloud.baseUrl = _prefs.getString("base_url", config.cloud.baseUrl);
  config.cloud.telemetryPath = _prefs.getString("tele_path", config.cloud.telemetryPath);
  config.cloud.authToken = _prefs.getString("auth_tok", config.cloud.authToken);
  config.cloud.reportIntervalMs = _prefs.getULong("report_ms", config.cloud.reportIntervalMs);
  config.cloud.retryIntervalMs = _prefs.getULong("retry_ms", config.cloud.retryIntervalMs);
  config.cloud.completePayload = _prefs.getBool("payload_full", config.cloud.completePayload);
  config.cloud.mcbeamCompatPayload = _prefs.getBool("payload_mcb", config.cloud.mcbeamCompatPayload);
  config.cloud.mqttEnabled = _prefs.getBool("mqtt_en", config.cloud.mqttEnabled);
  config.cloud.mqttHost = _prefs.getString("mqtt_host", config.cloud.mqttHost);
  config.cloud.mqttPort = static_cast<uint16_t>(_prefs.getUShort("mqtt_port", config.cloud.mqttPort));
  config.cloud.mqttTls = _prefs.getBool("mqtt_tls", config.cloud.mqttTls);
  config.cloud.mqttClientId = _prefs.getString("mqtt_cid", config.cloud.mqttClientId);
  config.cloud.mqttUsername = _prefs.getString("mqtt_user", config.cloud.mqttUsername);
  config.cloud.mqttPassword = _prefs.getString("mqtt_pass", config.cloud.mqttPassword);
  config.cloud.mqttTelemetryTopic = _prefs.getString("mqtt_topic", config.cloud.mqttTelemetryTopic);
  config.cloud.mqttCmdTopic = _prefs.getString("mqtt_cmd_t", config.cloud.mqttCmdTopic);
  config.cloud.mqttStatusTopic = _prefs.getString("mqtt_st_t", config.cloud.mqttStatusTopic);
  config.cloud.httpFallbackEnabled = _prefs.getBool("http_fb", config.cloud.httpFallbackEnabled);

  config.rs485.pzemEnabled = _prefs.getBool("pzem_en", config.rs485.pzemEnabled);
  config.rs485.pzemSlaveId = _prefs.getUChar("pzem_sid", config.rs485.pzemSlaveId);
  config.rs485.generatorEnabled = _prefs.getBool("gen_en", config.rs485.generatorEnabled);
  config.rs485.generatorCount = _prefs.getUChar("gen_cnt", config.rs485.generatorCount);
  if (config.rs485.generatorCount < 1) config.rs485.generatorCount = 1;
  if (config.rs485.generatorCount > Rs485Config::MAX_GENERATORS) config.rs485.generatorCount = Rs485Config::MAX_GENERATORS;
  for (uint8_t i = 0; i < Rs485Config::MAX_GENERATORS; ++i) {
    const String keySid = "gen_sid" + String(i);
    config.rs485.generatorSlaveIds[i] = _prefs.getUChar(keySid.c_str(), config.rs485.generatorSlaveIds[i]);
    const String keyModel = "gen_model" + String(i);
    config.rs485.generatorModels[i] = static_cast<GeneratorModel>(
      _prefs.getUChar(keyModel.c_str(), static_cast<uint8_t>(config.rs485.generatorModels[i])));
  }
  // Legacy migration: map previous single-generator keys into generator 1 when new keys are absent.
  if (!_prefs.isKey("gen_sid0") && _prefs.isKey("gen_sid")) {
    config.rs485.generatorSlaveIds[0] = _prefs.getUChar("gen_sid", config.rs485.generatorSlaveIds[0]);
  }
  if (!_prefs.isKey("gen_model0") && _prefs.isKey("gen_model")) {
    config.rs485.generatorModels[0] = static_cast<GeneratorModel>(
      _prefs.getUChar("gen_model", static_cast<uint8_t>(config.rs485.generatorModels[0])));
  }
  config.rs485.batteryEnabled = _prefs.getBool("batt_en", config.rs485.batteryEnabled);
  config.rs485.rectifierCount = _prefs.getUChar("rect_cnt", config.rs485.rectifierCount);
  config.rs485.batteryBankCount = _prefs.getUChar("batt_cnt", config.rs485.batteryBankCount);
  for (uint8_t i = 0; i < Rs485Config::MAX_BATTERY_BANKS; ++i) {
    const String key = "batt_sid" + String(i);
    config.rs485.batteryBankSlaveIds[i] = _prefs.getUChar(key.c_str(), config.rs485.batteryBankSlaveIds[i]);
    const String keyModel = "batt_model" + String(i);
    config.rs485.batteryBankModels[i] = static_cast<BatteryModel>(
      _prefs.getUChar(keyModel.c_str(), static_cast<uint8_t>(config.rs485.batteryBankModels[i])));
  }
  config.rs485.baudRate = _prefs.getULong("rs485_baud", config.rs485.baudRate);

  config.fuel.enabled = _prefs.getBool("fuel_en", config.fuel.enabled);
  config.fuel.raw0 = _prefs.getInt("fuel_r0", config.fuel.raw0);
  config.fuel.raw25 = _prefs.getInt("fuel_r25", config.fuel.raw25);
  config.fuel.raw50 = _prefs.getInt("fuel_r50", config.fuel.raw50);
  config.fuel.raw75 = _prefs.getInt("fuel_r75", config.fuel.raw75);
  config.fuel.raw100 = _prefs.getInt("fuel_r100", config.fuel.raw100);
  config.fuel.tankLengthCm = _prefs.getFloat("fuel_tank_l", config.fuel.tankLengthCm);
  config.fuel.tankDiameterCm = _prefs.getFloat("fuel_tank_d", config.fuel.tankDiameterCm);
  config.fuel.sensorReachHeightCm = _prefs.getFloat("fuel_sens_h", config.fuel.sensorReachHeightCm);
  config.fuel.sensorUnreachedHeightCm = _prefs.getFloat("fuel_unrch_h", config.fuel.sensorUnreachedHeightCm);
  config.fuel.deadSpaceLiters = _prefs.getFloat("fuel_dead", config.fuel.deadSpaceLiters);

  config.alarms.acMains.activeHigh = _prefs.getBool("al_ac_hi", config.alarms.acMains.activeHigh);
  config.alarms.gensetRun.activeHigh = _prefs.getBool("al_gr_hi", config.alarms.gensetRun.activeHigh);
  config.alarms.gensetFail.activeHigh = _prefs.getBool("al_gf_hi", config.alarms.gensetFail.activeHigh);
  config.alarms.batteryTheft.activeHigh = _prefs.getBool("al_bt_hi", config.alarms.batteryTheft.activeHigh);
  config.alarms.powerCableTheft.activeHigh = _prefs.getBool("al_pc_hi", config.alarms.powerCableTheft.activeHigh);
  config.alarms.doorOpen.activeHigh = _prefs.getBool("al_do_hi", config.alarms.doorOpen.activeHigh);

  config.ui.apSsid = _prefs.getString("ap_ssid", config.ui.apSsid);
  config.ui.apPassword = _prefs.getString("ap_pass", config.ui.apPassword);
  config.ui.adminUser = _prefs.getString("ui_user", config.ui.adminUser);
  config.ui.adminPassword = _prefs.getString("ui_pass", config.ui.adminPassword);
  return true;
}

bool PreferencesStore::save(const DeviceConfig& config) {
  _prefs.putString("device_id", config.identity.deviceId);
  _prefs.putString("site_id", config.identity.siteId);
  _prefs.putString("site_name", config.identity.siteName);

  _prefs.putString("base_url", config.cloud.baseUrl);
  _prefs.putString("tele_path", config.cloud.telemetryPath);
  _prefs.putString("auth_tok", config.cloud.authToken);
  _prefs.putULong("report_ms", config.cloud.reportIntervalMs);
  _prefs.putULong("retry_ms", config.cloud.retryIntervalMs);
  _prefs.putBool("payload_full", config.cloud.completePayload);
  _prefs.putBool("payload_mcb", config.cloud.mcbeamCompatPayload);
  _prefs.putBool("mqtt_en", config.cloud.mqttEnabled);
  _prefs.putString("mqtt_host", config.cloud.mqttHost);
  _prefs.putUShort("mqtt_port", config.cloud.mqttPort);
  _prefs.putBool("mqtt_tls", config.cloud.mqttTls);
  _prefs.putString("mqtt_cid", config.cloud.mqttClientId);
  _prefs.putString("mqtt_user", config.cloud.mqttUsername);
  _prefs.putString("mqtt_pass", config.cloud.mqttPassword);
  _prefs.putString("mqtt_topic", config.cloud.mqttTelemetryTopic);
  _prefs.putString("mqtt_cmd_t", config.cloud.mqttCmdTopic);
  _prefs.putString("mqtt_st_t", config.cloud.mqttStatusTopic);
  _prefs.putBool("http_fb", config.cloud.httpFallbackEnabled);

  _prefs.putBool("pzem_en", config.rs485.pzemEnabled);
  _prefs.putUChar("pzem_sid", config.rs485.pzemSlaveId);
  _prefs.putBool("gen_en", config.rs485.generatorEnabled);
  _prefs.putUChar("gen_cnt", config.rs485.generatorCount);
  for (uint8_t i = 0; i < Rs485Config::MAX_GENERATORS; ++i) {
    const String keySid = "gen_sid" + String(i);
    _prefs.putUChar(keySid.c_str(), config.rs485.generatorSlaveIds[i]);
    const String keyModel = "gen_model" + String(i);
    _prefs.putUChar(keyModel.c_str(), static_cast<uint8_t>(config.rs485.generatorModels[i]));
  }
  _prefs.remove("gen_sid");
  _prefs.remove("gen_model");
  _prefs.putBool("batt_en", config.rs485.batteryEnabled);
  _prefs.putUChar("rect_cnt", config.rs485.rectifierCount);
  _prefs.putUChar("batt_cnt", config.rs485.batteryBankCount);
  for (uint8_t i = 0; i < Rs485Config::MAX_BATTERY_BANKS; ++i) {
    const String key = "batt_sid" + String(i);
    _prefs.putUChar(key.c_str(), config.rs485.batteryBankSlaveIds[i]);
    const String keyModel = "batt_model" + String(i);
    _prefs.putUChar(keyModel.c_str(), static_cast<uint8_t>(config.rs485.batteryBankModels[i]));
  }
  _prefs.putULong("rs485_baud", config.rs485.baudRate);

  _prefs.putBool("fuel_en", config.fuel.enabled);
  _prefs.putInt("fuel_r0", config.fuel.raw0);
  _prefs.putInt("fuel_r25", config.fuel.raw25);
  _prefs.putInt("fuel_r50", config.fuel.raw50);
  _prefs.putInt("fuel_r75", config.fuel.raw75);
  _prefs.putInt("fuel_r100", config.fuel.raw100);
  _prefs.putFloat("fuel_tank_l", config.fuel.tankLengthCm);
  _prefs.putFloat("fuel_tank_d", config.fuel.tankDiameterCm);
  _prefs.putFloat("fuel_sens_h", config.fuel.sensorReachHeightCm);
  _prefs.putFloat("fuel_unrch_h", config.fuel.sensorUnreachedHeightCm);
  _prefs.putFloat("fuel_dead", config.fuel.deadSpaceLiters);
  // Cleanup legacy fuel keys no longer used by the current calibration model.
  const char* legacyFuelKeys[] = {"fuel_emp", "fuel_full", "fuel_l_full", "fuel_tank_w", "fuel_tank_h"};
  for (const char* key : legacyFuelKeys) {
    if (_prefs.isKey(key)) {
      _prefs.remove(key);
    }
  }

  _prefs.putBool("al_ac_hi", config.alarms.acMains.activeHigh);
  _prefs.putBool("al_gr_hi", config.alarms.gensetRun.activeHigh);
  _prefs.putBool("al_gf_hi", config.alarms.gensetFail.activeHigh);
  _prefs.putBool("al_bt_hi", config.alarms.batteryTheft.activeHigh);
  _prefs.putBool("al_pc_hi", config.alarms.powerCableTheft.activeHigh);
  _prefs.putBool("al_do_hi", config.alarms.doorOpen.activeHigh);

  _prefs.putString("ap_ssid", config.ui.apSsid);
  _prefs.putString("ap_pass", config.ui.apPassword);
  _prefs.putString("ui_user", config.ui.adminUser);
  _prefs.putString("ui_pass", config.ui.adminPassword);
  return true;
}
