#include "devices/DigitalInputs.h"
#include "Pins.h"
#include <Arduino.h>

bool DigitalInputs::begin(const AlarmConfig&) {
  pinMode(Pins::DI_AC_MAINS_RECTIFIER, INPUT_PULLUP);
  pinMode(Pins::DI_GENSET_OPERATION, INPUT_PULLUP);
  pinMode(Pins::DI_GENSET_FAILED, INPUT_PULLUP);
  pinMode(Pins::DI_BATTERY_THEFT, INPUT_PULLUP);
  pinMode(Pins::DI_POWER_CABLE_THEFT, INPUT_PULLUP);
  pinMode(Pins::DI_RS_DOOR_OPEN, INPUT_PULLUP);
  return true;
}

void DigitalInputs::update(const AlarmConfig& config) {
  _state.acMains = readInput(Pins::DI_AC_MAINS_RECTIFIER, config.acMains);
  _state.gensetRun = readInput(Pins::DI_GENSET_OPERATION, config.gensetRun);
  _state.gensetFail = readInput(Pins::DI_GENSET_FAILED, config.gensetFail);
  _state.batteryTheft = readInput(Pins::DI_BATTERY_THEFT, config.batteryTheft);
  _state.powerCableTheft = readInput(Pins::DI_POWER_CABLE_THEFT, config.powerCableTheft);
  _state.doorOpen = readInput(Pins::DI_RS_DOOR_OPEN, config.doorOpen);
}

const AlarmState& DigitalInputs::state() const {
  return _state;
}

bool DigitalInputs::readInput(int pin, const InputConfig& cfg) const {
  if (!cfg.enabled) {
    return false;
  }
  const bool raw = digitalRead(pin);
  return cfg.activeHigh ? raw : !raw;
}
