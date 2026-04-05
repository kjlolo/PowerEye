#pragma once
#include <ESPAsyncWebServer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "config/DeviceConfig.h"
#include "storage/PreferencesStore.h"
#include "models/TelemetrySnapshot.h"
#include "storage/QueueStore.h"

class WebUI {
public:
  WebUI(DeviceConfig& config, PreferencesStore& prefs, TelemetrySnapshot& snapshot, QueueStore& queue);
  void begin();
  bool consumeNetworkDiagRequest();
  void setNetworkDiagResult(const String& resultJson);
  String networkDiagStatusJson();
  bool consumeAwsTestRequest();
  void setAwsTestResult(const String& resultJson);
  String awsTestStatusJson();

private:
  AsyncWebServer _server;
  DeviceConfig& _config;
  PreferencesStore& _prefs;
  TelemetrySnapshot& _snapshot;
  QueueStore& _queue;

  SemaphoreHandle_t _diagMutex = nullptr;
  bool _networkDiagRequested = false;
  bool _networkDiagRunning = false;
  String _networkDiagLastResult = "{}";
  unsigned long _networkDiagUpdatedMs = 0;

  bool _awsTestRequested = false;
  bool _awsTestRunning = false;
  String _awsTestLastResult = "{}";
  unsigned long _awsTestUpdatedMs = 0;

  bool _otaUploadOk = false;
  String _otaUploadError;

  String dashboardHtml() const;
  String settingsHtml() const;
  String firmwareHtml() const;
  bool isAuthorized(AsyncWebServerRequest* request) const;
};
