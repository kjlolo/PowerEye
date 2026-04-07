#pragma once
#include <Arduino.h>
#include "comms/Air780E.h"
#include "config/DeviceConfig.h"

class MqttClient {
public:
  explicit MqttClient(Air780E& modem);
  bool ensureControlChannel(const CloudConfig& cloud);
  bool publish(const CloudConfig& cloud, const String& payload);
  bool publishStatus(const CloudConfig& cloud, const String& statusPayload);
  bool pollCommand(String& topic, String& payload);

private:
  String resolveClientId(const CloudConfig& cloud) const;

  Air780E& _modem;
  bool _controlReady = false;
  bool _statusPublished = false;
  String _controlKey;
};
