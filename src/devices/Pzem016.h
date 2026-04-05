#pragma once
#include "models/EnergyData.h"
#include "comms/ModbusBus.h"
#include <Preferences.h>

class Pzem016 {
public:
  Pzem016(ModbusBus& bus, uint8_t slaveId);
  bool begin();
  void setSlaveId(uint8_t slaveId);
  bool poll();
  const EnergyData& data() const;

private:
  void loadEnergyState();
  void persistEnergyState(bool force = false);

  ModbusBus& _bus;
  uint8_t _slaveId;
  EnergyData _data;
  Preferences _prefs;
  bool _stateLoaded = false;
  float _energyOffsetKwh = 0.0f;
  float _lastMeterEnergyKwh = 0.0f;
  bool _hasLastMeter = false;
  unsigned long _lastPersistMs = 0;
};
