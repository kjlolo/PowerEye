#pragma once
#include <Arduino.h>

class Air780E {
public:
  explicit Air780E(HardwareSerial& serial);
  bool begin(int8_t rxPin, int8_t txPin, uint32_t baud = 115200, int8_t pwrKeyPin = -1);
  bool isNetworkReady();
  int rssi();
  String phoneNumber();
  String diagnosticsJson();
  bool postJson(const String& baseUrl, const String& path, const String& bearerToken, const String& payload, int& httpCode, String& responseBody);
  bool mqttPublish(const String& host,
                   uint16_t port,
                   bool useTls,
                   const String& clientId,
                   const String& username,
                   const String& password,
                   const String& topic,
                   const String& payload);
  void mqttDisconnect();
  String lastError() const;

private:
  bool ensurePoweredOn();
  bool probeAtAnyBaud(int8_t rxPin, int8_t txPin, uint32_t preferredBaud);
  bool probeAt(uint8_t attempts, uint32_t timeoutMs, uint32_t retryDelayMs);
  void pulsePwrKey();
  bool sendCommand(const String& command, const String& expect, uint32_t timeoutMs, String* out = nullptr);
  bool sendCommandAny(const String& command, const char* expect1, const char* expect2, uint32_t timeoutMs, String* out = nullptr);
  bool extractHttpAction(const String& line, int& method, int& code, int& len) const;
  String normalizeUrl(const String& baseUrl, const String& path) const;
  bool ensureMqttConnected(const String& host,
                           uint16_t port,
                           bool useTls,
                           const String& clientId,
                           const String& username,
                           const String& password);
  bool mqttSendData(const String& command, const String& data, const String& expect, uint32_t timeoutMs);
  void clearRx();

  HardwareSerial& _serial;
  int8_t _pwrKeyPin = -1;
  String _lastError;
  bool _mqttReady = false;
  String _mqttEndpoint;
};
