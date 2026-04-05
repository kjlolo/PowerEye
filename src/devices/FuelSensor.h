#pragma once
#include "models/FuelData.h"
#include "config/DeviceConfig.h"

class FuelSensor {
public:
  explicit FuelSensor(int adcPin);
  bool begin();
  bool poll(const FuelConfig& cfg);
  const FuelData& data() const;

private:
  int _adcPin;
  FuelData _data;
  bool _filterInit = false;
  float _rawEma = 0.0f;
  int _rawHist[5] = {0, 0, 0, 0, 0};
  uint8_t _rawHistCount = 0;
  uint8_t _rawHistIndex = 0;
};
