#pragma once
#include "comms/HttpClient.h"
#include "comms/MqttClient.h"
#include "storage/QueueStore.h"
#include "config/DeviceConfig.h"
#include "Types.h"

class PublishManager {
public:
  PublishManager(HttpClient& http, MqttClient& mqtt, QueueStore& queue, DeviceConfig& config);

  void enqueue(const String& payload);
  PublishResult process(bool networkOnline);
  int lastHttpCode() const;
  String lastResponse() const;
  String lastTransport() const;

private:
  HttpClient& _http;
  MqttClient& _mqtt;
  QueueStore& _queue;
  DeviceConfig& _config;
  int _lastHttpCode = 0;
  String _lastResponse;
  String _lastTransport = "none";
};
