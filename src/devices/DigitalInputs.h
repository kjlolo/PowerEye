#pragma once
#include "models/AlarmState.h"
#include "config/DeviceConfig.h"

class DigitalInputs {
public:
  bool begin(const AlarmConfig& config);
  void update(const AlarmConfig& config);
  const AlarmState& state() const;

private:
  AlarmState _state;
  bool readInput(int pin, const InputConfig& cfg) const;
};
