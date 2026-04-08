#pragma once
#include "models/GensetData.h"
#include "comms/ModbusBus.h"

class Hgm6100nc {
public:
  enum class Mode : uint8_t { AUTO = 0, MANUAL = 1, STOP = 2 };

  Hgm6100nc(ModbusBus& bus, uint8_t slaveId);
  void setSlaveId(uint8_t slaveId);
  bool poll();
  bool setMode(Mode mode);
  bool remoteStart();
  bool remoteStop();
  String lastError() const;
  const GensetData& data() const;

private:
  ModbusBus& _bus;
  uint8_t _slaveId;
  GensetData _data;
  String _lastError;
};
