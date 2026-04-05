#include "devices/Pzem016.h"
#include "AppConfig.h"

Pzem016::Pzem016(ModbusBus& bus, uint8_t slaveId)
  : _bus(bus), _slaveId(slaveId) {}

bool Pzem016::begin() {
  loadEnergyState();
  return true;
}

void Pzem016::setSlaveId(uint8_t slaveId) {
  _slaveId = slaveId;
}

bool Pzem016::poll() {
  uint16_t regs[10] = {};
  if (!_bus.readInputRegisters(_slaveId, 0x0000, 10, regs)) {
    _data.online = false;
    return false;
  }

  const uint32_t rawCurrent = (static_cast<uint32_t>(regs[2]) << 16) | regs[1];
  const uint32_t rawPower = (static_cast<uint32_t>(regs[4]) << 16) | regs[3];
  const uint32_t rawEnergy = (static_cast<uint32_t>(regs[6]) << 16) | regs[5];
  const float meterEnergyKwh = static_cast<float>(rawEnergy) / 1000.0f;

  if (_hasLastMeter) {
    if ((meterEnergyKwh + AppConfig::PZEM_RESET_DROP_MARGIN_KWH) < _lastMeterEnergyKwh) {
      // Meter rolled over or reset. Maintain monotonic cumulative energy.
      if (_lastMeterEnergyKwh >= AppConfig::PZEM_WRAP_DETECT_HIGH_KWH &&
          meterEnergyKwh <= AppConfig::PZEM_WRAP_DETECT_LOW_KWH) {
        _energyOffsetKwh += AppConfig::PZEM_ENERGY_WRAP_KWH;
      } else {
        _energyOffsetKwh += _lastMeterEnergyKwh;
      }
      persistEnergyState(true);
    }
  }
  _lastMeterEnergyKwh = meterEnergyKwh;
  _hasLastMeter = true;

  _data.voltage = static_cast<float>(regs[0]) / 10.0f;
  _data.current = static_cast<float>(rawCurrent) / 1000.0f;
  _data.power = static_cast<float>(rawPower) / 10.0f;
  _data.energyKwh = _energyOffsetKwh + meterEnergyKwh;
  _data.frequency = static_cast<float>(regs[7]) / 10.0f;
  _data.powerFactor = static_cast<float>(regs[8]) / 100.0f;
  _data.alarmStatus = (regs[9] != 0x0000);
  _data.online = true;
  persistEnergyState(false);
  return true;
}

const EnergyData& Pzem016::data() const {
  return _data;
}

void Pzem016::loadEnergyState() {
  if (_stateLoaded) {
    return;
  }
  if (!_prefs.begin("pzem_energy", false)) {
    _stateLoaded = true;
    return;
  }
  const bool hasOffsetKey = _prefs.isKey("offset_kwh");
  const bool hasLastKey = _prefs.isKey("last_kwh");
  const bool hasFlagKey = _prefs.isKey("has_last");

  _energyOffsetKwh = _prefs.getFloat("offset_kwh", 0.0f);
  _lastMeterEnergyKwh = _prefs.getFloat("last_kwh", 0.0f);
  _hasLastMeter = _prefs.getBool("has_last", false);
  _lastPersistMs = millis();
  _stateLoaded = true;

  // Ensure keys exist immediately so they are visible in NVS tools even before first timed persist.
  if (!hasOffsetKey) _prefs.putFloat("offset_kwh", _energyOffsetKwh);
  if (!hasLastKey) _prefs.putFloat("last_kwh", _lastMeterEnergyKwh);
  if (!hasFlagKey) _prefs.putBool("has_last", _hasLastMeter);
}

void Pzem016::persistEnergyState(bool force) {
  if (!_stateLoaded) {
    return;
  }
  const unsigned long now = millis();
  if (!force && (now - _lastPersistMs) < AppConfig::PZEM_ENERGY_PERSIST_MS) {
    return;
  }
  _prefs.putFloat("offset_kwh", _energyOffsetKwh);
  _prefs.putFloat("last_kwh", _lastMeterEnergyKwh);
  _prefs.putBool("has_last", _hasLastMeter);
  _lastPersistMs = now;
}
