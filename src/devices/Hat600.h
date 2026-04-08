#pragma once
#include "models/AtsData.h"
#include "comms/ModbusBus.h"

class Hat600 {
public:
  enum class Mode : uint8_t { AUTO = 0, MANUAL = 1 };
  enum class SwitchTarget : uint8_t { SOURCE1 = 0, SOURCE2 = 1, OPEN_BOTH = 2 };

  Hat600(ModbusBus& bus, uint8_t slaveId);
  void setSlaveId(uint8_t slaveId);
  bool poll();
  bool setMode(Mode mode);
  bool switchTo(SwitchTarget target);
  bool setPrioritySource1();
  bool setPrioritySource2();
  bool resetAlarm();
  bool remoteStartGenerator();
  bool remoteStopGenerator();
  String lastError() const;
  const AtsData& data() const;

private:
  ModbusBus& _bus;
  uint8_t _slaveId;
  AtsData _data;
  String _lastError;
};
