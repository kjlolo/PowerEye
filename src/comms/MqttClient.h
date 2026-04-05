#pragma once
#include <Arduino.h>
#include "comms/Air780E.h"
#include "config/DeviceConfig.h"

class MqttClient {
public:
  explicit MqttClient(Air780E& modem);
  bool publish(const CloudConfig& cloud, const String& payload);

private:
  Air780E& _modem;
};
