#pragma once
#include "models/AtsData.h"
#include "comms/ModbusBus.h"

class Hat600 {
public:
  Hat600(ModbusBus& bus, uint8_t slaveId);
  void setSlaveId(uint8_t slaveId);
  bool poll();
  const AtsData& data() const;

private:
  ModbusBus& _bus;
  uint8_t _slaveId;
  AtsData _data;
};
