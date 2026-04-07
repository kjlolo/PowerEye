#include "app/SiteController.h"
#include "AppConfig.h"
#include "Pins.h"
#include "BuildInfo.h"
#include "comms/ota_signing_pub.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include "mbedtls/base64.h"
#include "mbedtls/error.h"
#include "mbedtls/pk.h"

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

  String urlEncode(const String& value) {
    String out;
    out.reserve(value.length() * 3);
    const char* hex = "0123456789ABCDEF";
    for (size_t i = 0; i < value.length(); ++i) {
      const uint8_t c = static_cast<uint8_t>(value.charAt(i));
      const bool safe = (c >= 'a' && c <= 'z') ||
                        (c >= 'A' && c <= 'Z') ||
                        (c >= '0' && c <= '9') ||
                        c == '-' || c == '_' || c == '.' || c == '~';
      if (safe) {
        out += static_cast<char>(c);
      } else {
        out += '%';
        out += hex[(c >> 4) & 0x0F];
        out += hex[c & 0x0F];
      }
    }
    return out;
  }

  String toLowerHex(const uint8_t* bytes, size_t len) {
    const char* hex = "0123456789abcdef";
    String out;
    out.reserve(len * 2);
    for (size_t i = 0; i < len; ++i) {
      out += hex[(bytes[i] >> 4) & 0x0F];
      out += hex[bytes[i] & 0x0F];
    }
    return out;
  }

  bool verifyOtaSignature(const uint8_t sha256[32], const String& signatureB64, String& errorOut) {
    errorOut = "";
    if (signatureB64.isEmpty()) {
      errorOut = "signature_empty";
      return false;
    }

    uint8_t signature[256];
    size_t signatureLen = 0;
    const int decodeRet = mbedtls_base64_decode(
      signature,
      sizeof(signature),
      &signatureLen,
      reinterpret_cast<const unsigned char*>(signatureB64.c_str()),
      signatureB64.length()
    );
    if (decodeRet != 0 || signatureLen == 0) {
      errorOut = "signature_decode_failed";
      return false;
    }

    mbedtls_pk_context pk;
    mbedtls_pk_init(&pk);
    const int parseRet = mbedtls_pk_parse_public_key(
      &pk,
      reinterpret_cast<const unsigned char*>(OTA_SIGNING_PUB_KEY),
      strlen(OTA_SIGNING_PUB_KEY) + 1
    );
    if (parseRet != 0) {
      char errbuf[96] = {0};
      mbedtls_strerror(parseRet, errbuf, sizeof(errbuf));
      errorOut = String("pubkey_parse_failed:") + String(errbuf);
      mbedtls_pk_free(&pk);
      return false;
    }

    const int verifyRet = mbedtls_pk_verify(
      &pk,
      MBEDTLS_MD_SHA256,
      sha256,
      32,
      signature,
      signatureLen
    );
    mbedtls_pk_free(&pk);
    if (verifyRet != 0) {
      char errbuf[96] = {0};
      mbedtls_strerror(verifyRet, errbuf, sizeof(errbuf));
      errorOut = String("signature_invalid:") + String(errbuf);
      return false;
    }
    return true;
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
  _snapshot.group = _config.identity.group;
  _snapshot.province = _config.identity.province;
  _snapshot.city = _config.identity.city;
  _deviceMac = _config.identity.deviceId;
  _deviceMac.replace(" ", "");
  _deviceMac.replace("-", "");
  _deviceMac.replace("_", "");
  _deviceMac.toLowerCase();
  _snapshot.deviceMac = _deviceMac;
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
      const String payload = _telemetryBuilder.buildJson(
        _snapshot,
        _config.cloud.completePayload,
        _config.cloud.mcbeamCompatPayload
      );
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
    result += "\"mqtt_mtls\":" + String(_config.cloud.mqttMtlsEnabled ? "true" : "false") + ",";
    result += "\"mqtt_tls_sni\":\"" + jsonEscape(_config.cloud.mqttTlsHostname) + "\",";
    result += "\"mqtt_topic\":\"" + jsonEscape(_config.cloud.mqttTelemetryTopic) + "\",";
    result += "\"mqtt_cmd_topic\":\"" + jsonEscape(_config.cloud.mqttCmdTopic) + "\",";
    result += "\"mqtt_status_topic\":\"" + jsonEscape(_config.cloud.mqttStatusTopic) + "\",";
    result += "\"mqtt_client_id\":\"" + jsonEscape(_config.cloud.mqttClientId) + "\",";
    result += "\"mqtt_username\":\"" + jsonEscape(_config.cloud.mqttUsername) + "\"";
    result += "}";
    _webUi.setMqttTestResult(result);
  }
  handlePolling();
  updateSnapshot();
  handleMqttControl();
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
  _snapshot.deviceMac = _deviceMac;
  _snapshot.siteId = _config.identity.siteId;
  _snapshot.siteName = _config.identity.siteName;
  _snapshot.group = _config.identity.group;
  _snapshot.province = _config.identity.province;
  _snapshot.city = _config.identity.city;
  _snapshot.sequenceId = _sequenceCounter;
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
  _snapshot.cfgPzemEnabled = _config.rs485.pzemEnabled;
  _snapshot.cfgGeneratorEnabled = _config.rs485.generatorEnabled;
  _snapshot.cfgBatteryEnabled = _config.rs485.batteryEnabled;
  _snapshot.cfgFuelEnabled = _config.fuel.enabled;
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
    _sequenceCounter++;
    _snapshot.sequenceId = _sequenceCounter;
    const String payload = _telemetryBuilder.buildJson(
      _snapshot,
      _config.cloud.completePayload,
      _config.cloud.mcbeamCompatPayload
    );
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

void SiteController::handleMqttControl() {
  if (!_config.cloud.mqttEnabled) {
    return;
  }
  if (_config.cloud.mqttHost.isEmpty() || _config.cloud.mqttTelemetryTopic.isEmpty()) {
    return;
  }
  if (_config.cloud.mqttClientId.isEmpty()) {
    _config.cloud.mqttClientId = _config.identity.deviceId;
  }

  const unsigned long now = millis();
  if (now - _lastMqttControlPoll < 500) {
    return;
  }
  _lastMqttControlPoll = now;

  if (!_mqtt.ensureControlChannel(_config.cloud)) {
    _mqttOnlinePublished = false;
    return;
  }

  if (!_mqttOnlinePublished && !_config.cloud.mqttStatusTopic.isEmpty()) {
    if (_mqtt.publishStatus(_config.cloud, "online")) {
      _mqttOnlinePublished = true;
    }
  }

  String topic;
  String payload;
  while (_mqtt.pollCommand(topic, payload)) {
    handleMqttCommand(topic, payload);
  }

  runPendingOtaCheck();
}

void SiteController::handleMqttCommand(const String& topic, const String& payload) {
  JsonDocument doc;
  if (deserializeJson(doc, payload) != DeserializationError::Ok) {
    return;
  }
  const String cmd = doc["cmd"] | "";
  if (cmd.isEmpty()) {
    return;
  }

  if (cmd == "ota_check") {
    Serial.printf("[MQTT] CMD ota_check topic=%s\n", topic.c_str());
    _pendingOtaCheck = true;
    return;
  }

  if (cmd == "sync_now") {
    const String requestId = doc["request_id"] | "";
    if (requestId.isEmpty()) {
      Serial.println("[MQTT] sync_now ignored: missing request_id");
      return;
    }
    if (requestId == _lastHandledSyncRequestId) {
      Serial.printf("[MQTT] sync_now duplicate ignored: %s\n", requestId.c_str());
      return;
    }
    if (millis() - _lastSyncNowMs < 2000) {
      Serial.println("[MQTT] sync_now ignored: rate-limited");
      return;
    }

    _sequenceCounter++;
    _snapshot.sequenceId = _sequenceCounter;
    const String syncPayload = _telemetryBuilder.buildJson(
      _snapshot,
      _config.cloud.completePayload,
      _config.cloud.mcbeamCompatPayload,
      true,
      requestId
    );
    if (_mqtt.publish(_config.cloud, syncPayload)) {
      _lastHandledSyncRequestId = requestId;
      _lastSyncNowMs = millis();
      Serial.printf("[MQTT] sync_now delivered for request_id=%s\n", requestId.c_str());
    } else {
      Serial.printf("[MQTT] sync_now publish failed for request_id=%s\n", requestId.c_str());
    }
  }
}

void SiteController::runPendingOtaCheck() {
  if (!_pendingOtaCheck) {
    return;
  }
  _pendingOtaCheck = false;

  if (_config.cloud.baseUrl.isEmpty()) {
    Serial.println("[OTA] base_url missing; ota_check ignored");
    return;
  }
  if (_config.identity.siteId.isEmpty()) {
    Serial.println("[OTA] site_id missing; ota_check ignored");
    return;
  }

  const String manifestPath = "/api/ota?site_id=" + urlEncode(_config.identity.siteId) +
                              "&fw=" + urlEncode(_snapshot.fwVersion);
  int manifestCode = 0;
  String manifestBody;
  if (!_http.getText(_config.cloud.baseUrl, manifestPath, _config.cloud.authToken, manifestCode, manifestBody)) {
    Serial.printf("[OTA] manifest fetch failed code=%d modem_error=%s\n", manifestCode, _modem.lastError().c_str());
    return;
  }

  JsonDocument manifestDoc;
  if (deserializeJson(manifestDoc, manifestBody) != DeserializationError::Ok) {
    Serial.println("[OTA] manifest JSON parse failed");
    return;
  }

  const String otaUrl = manifestDoc["url"] | "";
  const String expectedSha = manifestDoc["sha256"] | "";
  const String signatureB64 = manifestDoc["signature"] | "";
  if (otaUrl.isEmpty()) {
    Serial.println("[OTA] no pending update");
    return;
  }
  if (expectedSha.isEmpty() || signatureB64.isEmpty()) {
    Serial.println("[OTA] manifest missing sha256/signature; update rejected");
    return;
  }

  size_t hashBytes = 0;
  uint8_t computedSha[32] = {0};
  if (!_http.computeSha256(otaUrl, "", hashBytes, computedSha)) {
    Serial.printf("[OTA] hash pass failed modem_error=%s\n", _modem.lastError().c_str());
    return;
  }
  const String computedShaHex = toLowerHex(computedSha, sizeof(computedSha));
  String expectedShaNorm = expectedSha;
  expectedShaNorm.trim();
  expectedShaNorm.toLowerCase();
  if (computedShaHex != expectedShaNorm) {
    Serial.printf("[OTA] sha mismatch expected=%s computed=%s bytes=%u\n",
                  expectedShaNorm.c_str(),
                  computedShaHex.c_str(),
                  static_cast<unsigned>(hashBytes));
    return;
  }

  String sigError;
  if (!verifyOtaSignature(computedSha, signatureB64, sigError)) {
    Serial.printf("[OTA] signature verify failed: %s\n", sigError.c_str());
    return;
  }

  size_t written = 0;
  if (!_http.downloadToUpdate(otaUrl, "", written)) {
    Serial.printf("[OTA] download/apply failed modem_error=%s\n", _modem.lastError().c_str());
    return;
  }

  Serial.printf("[OTA] secure update applied (%u bytes). Rebooting.\n", static_cast<unsigned>(written));
  delay(500);
  ESP.restart();
}
