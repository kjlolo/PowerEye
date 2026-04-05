#pragma once
#include "models/GensetData.h"
#include "comms/ModbusBus.h"

class Hgm6100nc {
public:
  Hgm6100nc(ModbusBus& bus, uint8_t slaveId);
  void setSlaveId(uint8_t slaveId);
  bool poll();
  const GensetData& data() const;

private:
  ModbusBus& _bus;
  uint8_t _slaveId;
  GensetData _data;
};
