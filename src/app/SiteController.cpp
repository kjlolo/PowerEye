#include "app/SiteController.h"
#include "AppConfig.h"
#include "Pins.h"
#include "BuildInfo.h"
#include <Arduino.h>

namespace {
  HardwareSerial& MODEM_SERIAL = Serial2;
  HardwareSerial& RS485_SERIAL = Serial1;

  String jsonEscape(const String& in) {
    String out;
    out.reserve(in.length() + 8);
    for (size_t i = 0; i < in.length(); ++i) {
      const char c = in.charAt(i);
      if (c == '\\') out += "\\\\";
      else if (c == '"') out += "\\\"";
      else if (c == '\r') out += " ";
      else if (c == '\n') out += " ";
      else out += c;
    }
    out.trim();
    return out;
  }

  int firstActiveBatteryBankIndex(const DeviceConfig& config) {
    const uint8_t count = config.rs485.batteryBankCount;
    for (uint8_t i = 0; i < count; ++i) {
      const uint8_t sid = config.rs485.batteryBankSlaveIds[i];
      if (sid < 1 || sid > 247) continue;
      if (config.rs485.batteryBankModels[i] == BatteryModel::NONE) continue;
      return i;
    }
    return -1;
  }

  uint8_t selectedBatterySlaveId(const DeviceConfig& config) {
    const int idx = firstActiveBatteryBankIndex(config);
    if (idx >= 0) {
      const uint8_t sid = config.rs485.batteryBankSlaveIds[idx];
      if (sid >= 1 && sid <= 247) {
        return sid;
      }
    }
    return 0;
  }

  BatteryModel selectedBatteryModel(const DeviceConfig& config) {
    const int idx = firstActiveBatteryBankIndex(config);
    if (idx >= 0) {
      return config.rs485.batteryBankModels[idx];
    }
    return BatteryModel::NONE;
  }

  int firstActiveGeneratorIndex(const DeviceConfig& config) {
    const uint8_t count = config.rs485.generatorCount;
    for (uint8_t i = 0; i < count; ++i) {
      const uint8_t sid = config.rs485.generatorSlaveIds[i];
      if (sid < 1 || sid > 247) continue;
      if (config.rs485.generatorModels[i] == GeneratorModel::NONE) continue;
      return i;
    }
    return -1;
  }

  uint8_t selectedGeneratorSlaveId(const DeviceConfig& config) {
    const int idx = firstActiveGeneratorIndex(config);
    if (idx >= 0) {
      const uint8_t sid = config.rs485.generatorSlaveIds[idx];
      if (sid >= 1 && sid <= 247) {
        return sid;
      }
    }
    return 0;
  }

  GeneratorModel selectedGeneratorModel(const DeviceConfig& config) {
    const int idx = firstActiveGeneratorIndex(config);
    if (idx >= 0) {
      return config.rs485.generatorModels[idx];
    }
    return GeneratorModel::NONE;
  }

  bool isGridAvailableFromPzem(const DeviceConfig& config, const EnergyData& energy) {
    if (!config.rs485.pzemEnabled) {
      return false;
    }
    return energy.online && energy.voltage > 150.0f;
  }

  bool isGensetRunningFromModule(const GensetData& g) {
    if (!g.online) {
      return false;
    }
    if (g.activePowerKw > 0.2f || g.speedRpm > 200.0f) {
      return true;
    }
    if (g.generatorLoad || g.generatorState != 0) {
      return true;
    }
    return false;
  }

  bool isGensetFailureFromModule(const GensetData& g) {
    if (!g.online) {
      return false;
    }
    return g.commonAlarm ||
           g.commonShutdown ||
           g.failedToStartShutdown ||
           g.overSpeedShutdown ||
           g.lowOilPressureShutdown ||
           g.highEngineTemperatureShutdown;
  }

}

SiteController::SiteController()
  : _modbus(RS485_SERIAL, Pins::RS485_DE_RE),
    _modem(MODEM_SERIAL),
    _http(_modem),
    _mqtt(_modem),
    _pzem(_modbus, 1),
    _genset(_modbus, 2),
    _battery(_modbus, 3),
    _fuel(Pins::FUEL_ADC),
    _publishManager(_http, _mqtt, _queue, _config),
    _webUi(_config, _prefs, _snapshot, _queue) {}

void SiteController::begin() {
  Serial.begin(AppConfig::SERIAL_BAUD);
  delay(300);
  Serial.println();
  Serial.println(AppConfig::PROJECT_NAME);

  _prefs.begin();
  _prefs.load(_config);
  _config.ui.apSsid = buildApSsidFromSiteId(_config.identity.siteId);

  _modbus.begin(_config.rs485.baudRate, Pins::RS485_RX, Pins::RS485_TX);
  const bool modemReady = _modem.begin(Pins::MODEM_RX, Pins::MODEM_TX, 115200, Pins::MODEM_PWRKEY);
  Serial.printf("[MODEM] init=%s lastError=%s\n", modemReady ? "OK" : "FAIL", _modem.lastError().c_str());
  _pzem.setSlaveId(_config.rs485.pzemSlaveId);
  _genset.setSlaveId(selectedGeneratorSlaveId(_config));
  _battery.configure(selectedBatteryModel(_config), selectedBatterySlaveId(_config));
  _pzem.begin();
  _fuel.begin();
  _inputs.begin(_config.alarms);
  _webUi.begin();

  _snapshot.deviceId = _config.identity.deviceId;
  _snapshot.siteId = _config.identity.siteId;
  _snapshot.siteName = _config.identity.siteName;
}

void SiteController::update() {
  if (_webUi.consumeNetworkDiagRequest()) {
    _webUi.setNetworkDiagResult(_modem.diagnosticsJson());
  }
  if (_webUi.consumeAwsTestRequest()) {
    int httpCode = 0;
    String responseBody;
    bool ok = false;
    String reason;
    if (_config.cloud.baseUrl.isEmpty() || _config.cloud.telemetryPath.isEmpty()) {
      reason = "missing_cloud_config";
    } else {
      const String payload = _telemetryBuilder.buildJson(_snapshot, _config.cloud.completePayload);
      ok = _http.postJson(_config.cloud.baseUrl, _config.cloud.telemetryPath, _config.cloud.authToken, payload, httpCode, responseBody);
      reason = ok ? "success" : "post_failed";
    }
    String result = "{";
    result += "\"ok\":" + String(ok ? "true" : "false") + ",";
    result += "\"reason\":\"" + jsonEscape(reason) + "\",";
    result += "\"http_code\":" + String(httpCode) + ",";
    result += "\"modem_error\":\"" + jsonEscape(_modem.lastError()) + "\",";
    result += "\"base_url\":\"" + jsonEscape(_config.cloud.baseUrl) + "\",";
    result += "\"telemetry_path\":\"" + jsonEscape(_config.cloud.telemetryPath) + "\",";
    result += "\"response\":\"" + jsonEscape(responseBody) + "\"";
    result += "}";
    _webUi.setAwsTestResult(result);
  }
  if (_webUi.consumeMqttTestRequest()) {
    bool ok = false;
    String reason;
    if (!_config.cloud.mqttEnabled) {
      reason = "mqtt_disabled";
    } else if (_config.cloud.mqttHost.isEmpty()) {
      reason = "mqtt_host_missing";
    } else if (_config.cloud.mqttTelemetryTopic.isEmpty()) {
      reason = "mqtt_topic_missing";
    } else {
      String probePayload = "{";
      probePayload += "\"type\":\"mqtt_test\",";
      probePayload += "\"site_id\":\"" + jsonEscape(_config.identity.siteId) + "\",";
      probePayload += "\"device_id\":\"" + jsonEscape(_config.identity.deviceId) + "\",";
      probePayload += "\"ts_ms\":" + String(millis());
      probePayload += "}";
      ok = _mqtt.publish(_config.cloud, probePayload);
      reason = ok ? "success" : "publish_failed";
    }
    String result = "{";
    result += "\"ok\":" + String(ok ? "true" : "false") + ",";
    result += "\"reason\":\"" + jsonEscape(reason) + "\",";
    result += "\"modem_error\":\"" + jsonEscape(_modem.lastError()) + "\",";
    result += "\"mqtt_enabled\":" + String(_config.cloud.mqttEnabled ? "true" : "false") + ",";
    result += "\"mqtt_host\":\"" + jsonEscape(_config.cloud.mqttHost) + "\",";
    result += "\"mqtt_port\":" + String(_config.cloud.mqttPort) + ",";
    result += "\"mqtt_tls\":" + String(_config.cloud.mqttTls ? "true" : "false") + ",";
    result += "\"mqtt_topic\":\"" + jsonEscape(_config.cloud.mqttTelemetryTopic) + "\",";
    result += "\"mqtt_client_id\":\"" + jsonEscape(_config.cloud.mqttClientId) + "\",";
    result += "\"mqtt_username\":\"" + jsonEscape(_config.cloud.mqttUsername) + "\"";
    result += "}";
    _webUi.setMqttTestResult(result);
  }
  handlePolling();
  updateSnapshot();
  handlePublishing();
}

void SiteController::handlePolling() {
  const unsigned long now = millis();
  _pzem.setSlaveId(_config.rs485.pzemSlaveId);
  _genset.setSlaveId(selectedGeneratorSlaveId(_config));
  _battery.configure(selectedBatteryModel(_config), selectedBatterySlaveId(_config));

  if (now - _lastPzemPoll >= AppConfig::PZEM_POLL_INTERVAL_MS) {
    _lastPzemPoll = now;
    if (_config.rs485.pzemEnabled) {
      _pzem.poll();
    }
  }

  if (now - _lastFuelPoll >= AppConfig::FUEL_POLL_INTERVAL_MS) {
    _lastFuelPoll = now;
    if (_config.fuel.enabled) {
      _fuel.poll(_config.fuel);
    }
  }

  if (now - _lastGensetPoll >= AppConfig::GENSET_POLL_INTERVAL_MS) {
    _lastGensetPoll = now;
    if (_config.rs485.generatorEnabled) {
      for (uint8_t i = 0; i < Rs485Config::MAX_GENERATORS; ++i) {
        _snapshot.gensets[i] = GensetData{};
      }
      const uint8_t count = _config.rs485.generatorCount > Rs485Config::MAX_GENERATORS
        ? Rs485Config::MAX_GENERATORS
        : _config.rs485.generatorCount;
      for (uint8_t i = 0; i < count; ++i) {
        const uint8_t sid = _config.rs485.generatorSlaveIds[i];
        const GeneratorModel model = _config.rs485.generatorModels[i];
        if (sid < 1 || sid > 247 || model == GeneratorModel::NONE) {
          _snapshot.gensets[i].online = false;
          continue;
        }
        _genset.setSlaveId(sid);
        if (model == GeneratorModel::HGM6100NC && _genset.poll()) {
          _snapshot.gensets[i] = _genset.data();
        } else {
          _snapshot.gensets[i].online = false;
        }
      }
    } else {
      for (uint8_t i = 0; i < Rs485Config::MAX_GENERATORS; ++i) {
        _snapshot.gensets[i] = GensetData{};
      }
    }
  }

  if (now - _lastBatteryPoll >= AppConfig::BATTERY_POLL_INTERVAL_MS) {
    _lastBatteryPoll = now;
    if (_config.rs485.batteryEnabled) {
      for (uint8_t i = 0; i < Rs485Config::MAX_BATTERY_BANKS; ++i) {
        _snapshot.batteryBanks[i] = BatteryData{};
      }
      const uint8_t count = _config.rs485.batteryBankCount > Rs485Config::MAX_BATTERY_BANKS
        ? Rs485Config::MAX_BATTERY_BANKS
        : _config.rs485.batteryBankCount;
      for (uint8_t i = 0; i < count; ++i) {
        const uint8_t sid = _config.rs485.batteryBankSlaveIds[i];
        const BatteryModel model = _config.rs485.batteryBankModels[i];
        if (sid < 1 || sid > 247 || model == BatteryModel::NONE) {
          _snapshot.batteryBanks[i].online = false;
          continue;
        }
        _battery.configure(model, sid);
        if (_battery.poll()) {
          _snapshot.batteryBanks[i] = _battery.data();
        } else {
          _snapshot.batteryBanks[i].online = false;
        }
      }
    } else {
      for (uint8_t i = 0; i < Rs485Config::MAX_BATTERY_BANKS; ++i) {
        _snapshot.batteryBanks[i] = BatteryData{};
      }
    }
  }

  if (now - _lastInputPoll >= AppConfig::DIGITAL_INPUT_POLL_MS) {
    _lastInputPoll = now;
    _inputs.update(_config.alarms);
    _alarmManager.update(_inputs.state());
  }
}

void SiteController::updateSnapshot() {
  _snapshot.deviceId = _config.identity.deviceId;
  _snapshot.siteId = _config.identity.siteId;
  _snapshot.siteName = _config.identity.siteName;
  _snapshot.uptimeMs = millis();
  _snapshot.queuePending = _queue.size();
  const unsigned long now = millis();
  if (now - _lastModemStatusPoll >= AppConfig::MODEM_STATUS_POLL_MS) {
    _lastModemStatusPoll = now;
    _cachedRssi = _modem.rssi();
    _cachedNetworkOnline = _modem.isNetworkReady();
    _cachedPhoneNumber = _modem.phoneNumber();
  }
  _snapshot.rssi = _cachedRssi;
  _snapshot.networkOnline = _cachedNetworkOnline;
  _snapshot.phoneNumber = _cachedPhoneNumber;
  _snapshot.fwVersion = AppConfig::FW_VERSION;
  _snapshot.transportStatus = _lastTransportStatus;
  _snapshot.lastError = _lastPublishError;
  _snapshot.energy = _pzem.data();
  _snapshot.gensetCountConfigured = _config.rs485.generatorCount > Rs485Config::MAX_GENERATORS
    ? Rs485Config::MAX_GENERATORS
    : _config.rs485.generatorCount;
  for (uint8_t i = 0; i < Rs485Config::MAX_GENERATORS; ++i) {
    _snapshot.gensetSlaveIds[i] = _config.rs485.generatorSlaveIds[i];
    _snapshot.gensetModels[i] = _config.rs485.generatorModels[i];
  }
  _snapshot.batteryBankCountConfigured = _config.rs485.batteryBankCount > Rs485Config::MAX_BATTERY_BANKS
    ? Rs485Config::MAX_BATTERY_BANKS
    : _config.rs485.batteryBankCount;
  for (uint8_t i = 0; i < Rs485Config::MAX_BATTERY_BANKS; ++i) {
    _snapshot.batteryBankSlaveIds[i] = _config.rs485.batteryBankSlaveIds[i];
    _snapshot.batteryBankModels[i] = _config.rs485.batteryBankModels[i];
  }
  _snapshot.fuel = _fuel.data();
  _snapshot.alarms = _alarmManager.current();

  // Alarm source mapping:
  // - AC mains alarm from PZEM/grid state.
  // - Genset run/fail alarms from genset module data.
  // Other digital alarms (theft/door/etc.) remain from digital inputs.
  const bool hasConfiguredGenset = _config.rs485.generatorEnabled &&
                                   selectedGeneratorModel(_config) != GeneratorModel::NONE;
  if (_config.rs485.pzemEnabled) {
    _snapshot.alarms.acMains = !isGridAvailableFromPzem(_config, _snapshot.energy);
  }
  if (hasConfiguredGenset) {
    bool gensetRun = false;
    bool gensetFail = false;
    const uint8_t count = _snapshot.gensetCountConfigured > Rs485Config::MAX_GENERATORS
      ? Rs485Config::MAX_GENERATORS
      : _snapshot.gensetCountConfigured;
    for (uint8_t i = 0; i < count; ++i) {
      const GensetData& g = _snapshot.gensets[i];
      gensetRun = gensetRun || isGensetRunningFromModule(g);
      gensetFail = gensetFail || isGensetFailureFromModule(g);
    }
    _snapshot.alarms.gensetRun = gensetRun;
    _snapshot.alarms.gensetFail = gensetFail;
  }

  if (!_config.rs485.pzemEnabled) {
    _snapshot.energy.online = false;
  }
  if (!_config.rs485.generatorEnabled || selectedGeneratorModel(_config) == GeneratorModel::NONE) {
    for (uint8_t i = 0; i < Rs485Config::MAX_GENERATORS; ++i) {
      _snapshot.gensets[i].online = false;
    }
  }
  if (!_config.rs485.batteryEnabled || selectedBatteryModel(_config) == BatteryModel::NONE) {
    for (uint8_t i = 0; i < Rs485Config::MAX_BATTERY_BANKS; ++i) {
      _snapshot.batteryBanks[i].online = false;
    }
  }
  if (!_config.fuel.enabled) {
    _snapshot.fuel.online = false;
  }
}

void SiteController::handlePublishing() {
  const unsigned long now = millis();

  if (_config.cloud.mqttEnabled && _config.cloud.mqttClientId.isEmpty()) {
    _config.cloud.mqttClientId = _config.identity.deviceId;
  }

  if (now - _lastReport >= _config.cloud.reportIntervalMs) {
    _lastReport = now;
    const String payload = _telemetryBuilder.buildJson(_snapshot, _config.cloud.completePayload);
    _publishManager.enqueue(payload);
    Serial.println("[QUEUE] telemetry enqueued");
  }

  if (now - _lastRetry >= _config.cloud.retryIntervalMs) {
    _lastRetry = now;
    const PublishResult result = _publishManager.process(_snapshot.networkOnline);
    if (result == PublishResult::SUCCESS) {
      Serial.printf("[PUBLISH] transport=%s http_code=%d response=%s\n",
                    _publishManager.lastTransport().c_str(),
                    _publishManager.lastHttpCode(),
                    _publishManager.lastResponse().c_str());
      _lastTransportStatus = _publishManager.lastTransport();
      _lastPublishError = "";
    } else if (result == PublishResult::FAILED) {
      Serial.printf("[PUBLISH] FAILED transport=%s modem_error=%s\n",
                    _publishManager.lastTransport().c_str(),
                    _modem.lastError().c_str());
      _lastTransportStatus = _publishManager.lastTransport();
      _lastPublishError = _modem.lastError();
    }
  }
}
