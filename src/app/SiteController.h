#pragma once
#include "config/DeviceConfig.h"
#include "storage/PreferencesStore.h"
#include "storage/QueueStore.h"
#include "comms/ModbusBus.h"
#include "comms/Air780E.h"
#include "comms/HttpClient.h"
#include "comms/MqttClient.h"
#include "devices/Pzem016.h"
#include "devices/Hgm6100nc.h"
#include "devices/BmsMonitor.h"
#include "devices/FuelSensor.h"
#include "devices/DigitalInputs.h"
#include "app/AlarmManager.h"
#include "app/TelemetryBuilder.h"
#include "app/PublishManager.h"
#include "app/WebUI.h"
#include "models/TelemetrySnapshot.h"

class SiteController {
public:
  SiteController();
  void begin();
  void update();

private:
  void updateSnapshot();
  void handlePolling();
  void handlePublishing();
  void handleMqttControl();
  void handleMqttCommand(const String& topic, const String& payload);

  DeviceConfig _config;
  PreferencesStore _prefs;
  QueueStore _queue;
  TelemetrySnapshot _snapshot;

  ModbusBus _modbus;
  Air780E _modem;
  HttpClient _http;
  MqttClient _mqtt;
  Pzem016 _pzem;
  Hgm6100nc _genset;
  BmsMonitor _battery;
  FuelSensor _fuel;
  DigitalInputs _inputs;
  AlarmManager _alarmManager;
  TelemetryBuilder _telemetryBuilder;
  PublishManager _publishManager;
  WebUI _webUi;

  unsigned long _lastPzemPoll = 0;
  unsigned long _lastFuelPoll = 0;
  unsigned long _lastGensetPoll = 0;
  unsigned long _lastBatteryPoll = 0;
  unsigned long _lastInputPoll = 0;
  unsigned long _lastModemStatusPoll = 0;
  unsigned long _lastReport = 0;
  unsigned long _lastRetry = 0;
  bool _cachedNetworkOnline = false;
  int _cachedRssi = -113;
  String _cachedPhoneNumber = "";
  String _lastTransportStatus = "init";
  String _lastPublishError = "";
  unsigned long _lastMqttControlPoll = 0;
  unsigned long _lastSyncNowMs = 0;
  String _lastHandledSyncRequestId = "";
  bool _pendingOtaCheck = false;
  bool _mqttOnlinePublished = false;
};
