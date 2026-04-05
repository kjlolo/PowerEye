#include "devices/FuelSensor.h"
#include <Arduino.h>
#include <math.h>

namespace {
bool isMonotonic(const int* values, size_t count) {
  bool nonDecreasing = true;
  bool nonIncreasing = true;
  for (size_t i = 1; i < count; ++i) {
    if (values[i] < values[i - 1]) nonDecreasing = false;
    if (values[i] > values[i - 1]) nonIncreasing = false;
  }
  return nonDecreasing || nonIncreasing;
}

float clampf(float v, float lo, float hi) {
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}

float horizontalCylinderSegmentAreaCm2(float radiusCm, float heightCm);

float volumeLitersFromCm3(float cm3) {
  return cm3 / 1000.0f;
}

float calcHorizontalCylinderTotalLiters(const FuelConfig& cfg) {
  if (cfg.tankLengthCm <= 0.0f || cfg.tankDiameterCm <= 0.0f) {
    return 0.0f;
  }
  const float radius = cfg.tankDiameterCm / 2.0f;
  const float fullCm3 = static_cast<float>(M_PI) * radius * radius * cfg.tankLengthCm;
  return volumeLitersFromCm3(fullCm3);
}

float calcUnreachedHeightCm(const FuelConfig& cfg) {
  if (cfg.tankDiameterCm <= 0.0f) {
    return 0.0f;
  }
  float unreachedHeight = cfg.sensorUnreachedHeightCm;
  if (cfg.sensorReachHeightCm > 0.0f) {
    const float inferredUnreached = cfg.tankDiameterCm - cfg.sensorReachHeightCm;
    if (inferredUnreached >= 0.0f) {
      unreachedHeight = inferredUnreached;
    }
  }
  return clampf(unreachedHeight, 0.0f, cfg.tankDiameterCm);
}

float calcHorizontalCylinderLitersAtHeight(const FuelConfig& cfg, float liquidHeightCm) {
  if (cfg.tankLengthCm <= 0.0f || cfg.tankDiameterCm <= 0.0f) {
    return 0.0f;
  }
  const float radius = cfg.tankDiameterCm / 2.0f;
  const float h = clampf(liquidHeightCm, 0.0f, cfg.tankDiameterCm);
  const float area = horizontalCylinderSegmentAreaCm2(radius, h);
  return volumeLitersFromCm3(area * cfg.tankLengthCm);
}

float heightFromHorizontalCylinderLiters(const FuelConfig& cfg, float liters) {
  if (cfg.tankLengthCm <= 0.0f || cfg.tankDiameterCm <= 0.0f) {
    return 0.0f;
  }
  const float total = calcHorizontalCylinderTotalLiters(cfg);
  const float target = clampf(liters, 0.0f, total);

  float lo = 0.0f;
  float hi = cfg.tankDiameterCm;
  for (int i = 0; i < 24; ++i) {
    const float mid = (lo + hi) * 0.5f;
    const float midLiters = calcHorizontalCylinderLitersAtHeight(cfg, mid);
    if (midLiters < target) {
      lo = mid;
    } else {
      hi = mid;
    }
  }
  return (lo + hi) * 0.5f;
}

float horizontalCylinderSegmentAreaCm2(float radiusCm, float heightCm) {
  if (heightCm <= 0.0f) return 0.0f;
  const float diameter = 2.0f * radiusCm;
  if (heightCm >= diameter) return static_cast<float>(M_PI) * radiusCm * radiusCm;

  const float h = clampf(heightCm, 0.0f, diameter);
  const float term = (radiusCm - h) / radiusCm;
  const float acosTerm = acosf(clampf(term, -1.0f, 1.0f));
  const float sqrtTerm = sqrtf(fmaxf(0.0f, (2.0f * radiusCm * h) - (h * h)));
  return (radiusCm * radiusCm * acosTerm) - ((radiusCm - h) * sqrtTerm);
}

float calcDeadSpaceLiters(const FuelConfig& cfg) {
  if (cfg.tankLengthCm <= 0.0f || cfg.tankDiameterCm <= 0.0f) {
    return cfg.deadSpaceLiters;
  }
  const float clampedHeight = calcUnreachedHeightCm(cfg);
  const float radius = cfg.tankDiameterCm / 2.0f;
  const float segmentArea = horizontalCylinderSegmentAreaCm2(radius, clampedHeight);
  const float deadCm3 = segmentArea * cfg.tankLengthCm;
  return volumeLitersFromCm3(deadCm3);
}
}

FuelSensor::FuelSensor(int adcPin) : _adcPin(adcPin) {}

bool FuelSensor::begin() {
  analogReadResolution(12);
  pinMode(_adcPin, INPUT);
  return true;
}

bool FuelSensor::poll(const FuelConfig& cfg) {
  const int rawAdc = analogRead(_adcPin);
  int raw = rawAdc;

  const int calMin = min(cfg.raw0, cfg.raw100);
  const int calMax = max(cfg.raw0, cfg.raw100);

  // Detect disconnected/invalid sensor before clamping:
  // - floating/open input typically saturates near ADC max
  // - hard short to GND typically stays near ADC min
  // - values far outside calibrated span indicate invalid wiring/signal
  const bool railHigh = rawAdc >= 4090;
  const bool railLow = rawAdc <= 5;
  const bool farAboveCal = rawAdc > (calMax + 300);
  const bool farBelowCal = rawAdc < (calMin - 300);
  if (railHigh || railLow || farAboveCal || farBelowCal) {
    _data.raw = rawAdc;
    _data.percent = 0.0f;
    _data.liters = 0.0f;
    _data.online = false;
    _filterInit = false;
    _rawEma = 0.0f;
    _rawHistCount = 0;
    _rawHistIndex = 0;
    return false;
  }

  // Clamp to calibration span to avoid out-of-range spikes affecting interpolation.
  raw = constrain(raw, calMin, calMax);

  // Median filter to reject impulse noise from ADC/sensor wiring.
  _rawHist[_rawHistIndex] = raw;
  _rawHistIndex = static_cast<uint8_t>((_rawHistIndex + 1) % 5);
  if (_rawHistCount < 5) {
    _rawHistCount++;
  }

  int sorted[5] = {0, 0, 0, 0, 0};
  for (uint8_t i = 0; i < _rawHistCount; ++i) {
    sorted[i] = _rawHist[i];
  }
  for (uint8_t i = 0; i + 1 < _rawHistCount; ++i) {
    for (uint8_t j = static_cast<uint8_t>(i + 1); j < _rawHistCount; ++j) {
      if (sorted[j] < sorted[i]) {
        const int t = sorted[i];
        sorted[i] = sorted[j];
        sorted[j] = t;
      }
    }
  }
  const int rawMedian = sorted[_rawHistCount / 2];

  // EMA + slew limiter gives smooth tracking while keeping bounded step changes.
  const float alpha = 0.25f;
  const float maxStep = 80.0f;
  if (!_filterInit) {
    _rawEma = static_cast<float>(rawMedian);
    _filterInit = true;
  } else {
    const float target = _rawEma + (alpha * (static_cast<float>(rawMedian) - _rawEma));
    float delta = target - _rawEma;
    if (delta > maxStep) delta = maxStep;
    if (delta < -maxStep) delta = -maxStep;
    _rawEma += delta;
  }

  raw = static_cast<int>(roundf(_rawEma));
  _data.raw = raw;

  const int x[5] = {cfg.raw0, cfg.raw25, cfg.raw50, cfg.raw75, cfg.raw100};
  const float yPct[5] = {0.0f, 25.0f, 50.0f, 75.0f, 100.0f};

  if (!isMonotonic(x, 5)) {
    _data.percent = 0.0f;
    _data.liters = 0.0f;
    _data.online = false;
    return false;
  }

  const float totalLiters = calcHorizontalCylinderTotalLiters(cfg);
  const float deadSpace = calcDeadSpaceLiters(cfg);
  if (totalLiters <= 0.0f || totalLiters <= deadSpace) {
    _data.percent = 0.0f;
    _data.liters = 0.0f;
    _data.online = false;
    return false;
  }

  // Calibration anchors are based on sensor-height percentages of measurable stroke.
  const float unreachedHeight = calcUnreachedHeightCm(cfg);
  const float sensorStrokeHeight = clampf(cfg.tankDiameterCm - unreachedHeight, 0.0f, cfg.tankDiameterCm);
  if (sensorStrokeHeight <= 0.0f) {
    _data.percent = 0.0f;
    _data.liters = 0.0f;
    _data.online = false;
    return false;
  }

  const float measurableLiters = totalLiters - deadSpace;
  float yHeight[5];
  for (int i = 0; i < 5; ++i) {
    yHeight[i] = unreachedHeight + ((yPct[i] / 100.0f) * sensorStrokeHeight);
  }

  const bool ascending = x[4] >= x[0];
  float liquidHeightCm = yHeight[0];
  bool foundSegment = false;

  for (int i = 0; i < 4; ++i) {
    const int x0 = x[i];
    const int x1 = x[i + 1];
    if (x0 == x1) {
      continue;
    }

    const bool inSegment = ascending
      ? (raw >= x0 && raw <= x1)
      : (raw <= x0 && raw >= x1);

    if (inSegment) {
      const float t = static_cast<float>(raw - x0) / static_cast<float>(x1 - x0);
      liquidHeightCm = yHeight[i] + ((yHeight[i + 1] - yHeight[i]) * t);
      foundSegment = true;
      break;
    }
  }

  if (!foundSegment) {
    if (ascending) {
      liquidHeightCm = (raw <= x[0]) ? yHeight[0] : yHeight[4];
    } else {
      liquidHeightCm = (raw >= x[0]) ? yHeight[0] : yHeight[4];
    }
  }

  liquidHeightCm = clampf(liquidHeightCm, 0.0f, cfg.tankDiameterCm);
  const float liters = calcHorizontalCylinderLitersAtHeight(cfg, liquidHeightCm);
  float pct = ((liters - deadSpace) / measurableLiters) * 100.0f;
  pct = clampf(pct, 0.0f, 100.0f);

  _data.percent = pct;
  _data.liters = liters;
  _data.online = true;
  return true;
}

const FuelData& FuelSensor::data() const {
  return _data;
}
