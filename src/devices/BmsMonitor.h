#pragma once
#include "config/DeviceConfig.h"
#include "models/BatteryData.h"
#include "comms/ModbusBus.h"

class BmsMonitor {
public:
  BmsMonitor(ModbusBus& bus, uint8_t slaveId);
  void configure(BatteryModel model, uint8_t slaveId);
  bool poll();
  const BatteryData& data() const;

private:
  void decodeCommon(const uint16_t* regs);

  ModbusBus& _bus;
  BatteryModel _model = BatteryModel::NONE;
  uint8_t _slaveId;
  BatteryData _data;
};
