#pragma once

struct EnergyData {
  float voltage = 0.0f;
  float current = 0.0f;
  float power = 0.0f;
  float energyKwh = 0.0f;
  float frequency = 0.0f;
  float powerFactor = 0.0f;
  bool alarmStatus = false;
  bool online = false;
};
