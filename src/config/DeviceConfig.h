#pragma once
#include <Arduino.h>

enum class GeneratorModel : uint8_t {
  NONE = 0,
  HGM6100NC = 1,
};

enum class BatteryModel : uint8_t {
  NONE = 0,
  CHANGHONG = 1,
  WOLONG = 2,
};

inline const char* generatorModelToString(GeneratorModel model) {
  switch (model) {
    case GeneratorModel::HGM6100NC: return "hgm6100nc";
    case GeneratorModel::NONE:
    default: return "none";
  }
}

inline GeneratorModel generatorModelFromString(const String& value) {
  if (value.equalsIgnoreCase("hgm6100nc")) {
    return GeneratorModel::HGM6100NC;
  }
  return GeneratorModel::NONE;
}

inline const char* batteryModelToString(BatteryModel model) {
  switch (model) {
    case BatteryModel::CHANGHONG: return "changhong";
    case BatteryModel::WOLONG: return "wolong";
    case BatteryModel::NONE:
    default: return "none";
  }
}

inline BatteryModel batteryModelFromString(const String& value) {
  if (value.equalsIgnoreCase("changhong")) {
    return BatteryModel::CHANGHONG;
  }
  if (value.equalsIgnoreCase("wolong")) {
    return BatteryModel::WOLONG;
  }
  return BatteryModel::NONE;
}

struct DeviceIdentityConfig {
  String deviceId = "PE-001";
  String siteId = "SITE-001";
  String siteName = "UNASSIGNED";
};

struct CloudConfig {
  String baseUrl = "https://example.execute-api.ap-southeast-1.amazonaws.com";
  String telemetryPath = "/telemetry";
  String configPath = "/config";
  String authToken = "CHANGE_ME";
  uint32_t reportIntervalMs = 60000;
  uint32_t retryIntervalMs = 15000;
  bool completePayload = false;
  bool mcbeamCompatPayload = false;
  bool mqttEnabled = false;
  String mqttHost = "";
  uint16_t mqttPort = 8883;
  bool mqttTls = true;
  String mqttClientId = "";
  String mqttUsername = "";
  String mqttPassword = "";
  String mqttTelemetryTopic = "powereye/telemetry";
  String mqttCmdTopic = "powereye/cmd";
  String mqttStatusTopic = "powereye/status";
  bool mqttMtlsEnabled = false;
  String mqttTlsHostname = "";
  String mqttCaCertPem = "";
  String mqttClientCertPem = "";
  String mqttClientKeyPem = "";
  bool httpFallbackEnabled = true;
};

struct Rs485Config {
  static constexpr uint8_t MAX_GENERATORS = 4;
  static constexpr uint8_t MAX_BATTERY_BANKS = 16;
  static constexpr uint8_t BANKS_PER_RECTIFIER = 4;

  bool pzemEnabled = true;
  uint8_t pzemSlaveId = 1;
  bool generatorEnabled = false;
  uint8_t generatorCount = 1;
  uint8_t generatorSlaveIds[MAX_GENERATORS] = {2, 19, 20, 24};
  GeneratorModel generatorModels[MAX_GENERATORS] = {
    GeneratorModel::NONE, GeneratorModel::NONE, GeneratorModel::NONE, GeneratorModel::NONE
  };
  bool batteryEnabled = false;
  uint8_t rectifierCount = 1;
  uint8_t batteryBankCount = 4;
  uint8_t batteryBankSlaveIds[MAX_BATTERY_BANKS] = {3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18};
  BatteryModel batteryBankModels[MAX_BATTERY_BANKS] = {
    BatteryModel::NONE, BatteryModel::NONE, BatteryModel::NONE, BatteryModel::NONE,
    BatteryModel::NONE, BatteryModel::NONE, BatteryModel::NONE, BatteryModel::NONE,
    BatteryModel::NONE, BatteryModel::NONE, BatteryModel::NONE, BatteryModel::NONE,
    BatteryModel::NONE, BatteryModel::NONE, BatteryModel::NONE, BatteryModel::NONE
  };
  uint32_t baudRate = 9600;
};

struct FuelConfig {
  bool enabled = true;
  int raw0 = 800;
  int raw25 = 1350;
  int raw50 = 1900;
  int raw75 = 2450;
  int raw100 = 3000;
  float tankLitersAtFull = 200.0f;
  float tankLengthCm = 100.0f;
  float tankDiameterCm = 40.0f;
  float sensorReachHeightCm = 38.0f;
  float sensorUnreachedHeightCm = 2.0f;
  float deadSpaceLiters = 0.0f;
};

struct InputConfig {
  bool enabled = true;
  bool activeHigh = false;
  uint32_t debounceMs = 250;
};

struct AlarmConfig {
  InputConfig acMains;
  InputConfig gensetRun;
  InputConfig gensetFail;
  InputConfig batteryTheft;
  InputConfig powerCableTheft;
  InputConfig doorOpen;
};

struct UiConfig {
  String apSsid = "POWEREYE-PE001";
  String apPassword = "powereye123";
  String adminUser = "admin";
  String adminPassword = "admin";
};

inline String sanitizeSiteIdForApSsid(const String& siteId) {
  String sanitized;
  sanitized.reserve(siteId.length());
  for (size_t i = 0; i < siteId.length(); ++i) {
    const char c = siteId.charAt(i);
    const bool alphaNum = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9');
    if (alphaNum || c == '-' || c == '_') {
      sanitized += c;
    } else if (c == ' ' || c == '/' || c == '\\' || c == '.') {
      sanitized += '-';
    }
  }
  sanitized.trim();
  while (sanitized.indexOf("--") >= 0) {
    sanitized.replace("--", "-");
  }
  return sanitized;
}

inline String buildApSsidFromSiteId(const String& siteId) {
  String token = sanitizeSiteIdForApSsid(siteId);
  if (token.isEmpty()) {
    token = "SITE";
  }
  return "POWEREYE-" + token;
}

struct DeviceConfig {
  DeviceIdentityConfig identity;
  CloudConfig cloud;
  Rs485Config rs485;
  FuelConfig fuel;
  AlarmConfig alarms;
  UiConfig ui;
};
