#include "devices/Hgm6100nc.h"

namespace {
constexpr uint16_t REGISTER_BLOCK1_START = 0x0007;
constexpr uint16_t REGISTER_BLOCK1_COUNT = 0x0017;
constexpr uint16_t REGISTER_BLOCK2_START = 0x0022;
constexpr uint16_t REGISTER_BLOCK2_COUNT = 0x0011;
constexpr uint16_t COIL_START = 0x0000;
constexpr uint16_t COIL_COUNT = 0x0048;

uint32_t combineWords(uint16_t high, uint16_t low) {
  return (static_cast<uint32_t>(high) << 16) | static_cast<uint32_t>(low);
}
}

Hgm6100nc::Hgm6100nc(ModbusBus& bus, uint8_t slaveId)
  : _bus(bus), _slaveId(slaveId) {}

void Hgm6100nc::setSlaveId(uint8_t slaveId) {
  _slaveId = slaveId;
}

bool Hgm6100nc::poll() {
  uint16_t block1[REGISTER_BLOCK1_COUNT] = {};
  uint16_t block2[REGISTER_BLOCK2_COUNT] = {};
  bool coils[COIL_COUNT] = {};

  if (!_bus.readHoldingRegisters(_slaveId, REGISTER_BLOCK1_START, REGISTER_BLOCK1_COUNT, block1)) {
    _data.online = false;
    return false;
  }
  if (!_bus.readHoldingRegisters(_slaveId, REGISTER_BLOCK2_START, REGISTER_BLOCK2_COUNT, block2)) {
    _data.online = false;
    return false;
  }
  if (!_bus.readCoils(_slaveId, COIL_START, COIL_COUNT, coils)) {
    _data.online = false;
    return false;
  }

  _data.voltageA = static_cast<float>(block1[0]);
  _data.voltageB = static_cast<float>(block1[1]);
  _data.voltageC = static_cast<float>(block1[2]);
  _data.frequency = static_cast<float>(block1[6]) / 10.0f;
  _data.currentA = static_cast<float>(block1[7]) / 10.0f;
  _data.currentB = static_cast<float>(block1[8]) / 10.0f;
  _data.currentC = static_cast<float>(block1[9]) / 10.0f;
  _data.engineTemperature = static_cast<float>(block1[10]);
  _data.oilPressureKpa = static_cast<float>(block1[12]);
  _data.fuelLevelPercent = static_cast<float>(block1[14]);
  _data.speedRpm = static_cast<float>(block1[16]);
  _data.batteryVoltage = static_cast<float>(block1[17]) / 10.0f;
  _data.chargerVoltage = static_cast<float>(block1[18]) / 10.0f;
  _data.activePowerKw = static_cast<float>(block1[19]) / 10.0f;
  _data.reactivePowerKvar = static_cast<float>(block1[20]) / 10.0f;
  _data.apparentPowerKva = static_cast<float>(block1[21]) / 10.0f;
  _data.powerFactor = static_cast<float>(block1[22]) / 100.0f;

  _data.generatorState = block2[0];
  _data.mainsState = block2[6];
  _data.runHours = combineWords(block2[8], block2[9]);
  _data.startCount = combineWords(block2[12], block2[13]);
  _data.totalEnergy = combineWords(block2[14], block2[15]);

  _data.commonAlarm = coils[0x0000];
  _data.commonWarn = coils[0x0001];
  _data.commonShutdown = coils[0x0002];
  _data.generatorNormal = coils[0x0005];
  _data.mainsLoad = coils[0x0006];
  _data.generatorLoad = coils[0x0007];
  _data.overSpeedShutdown = coils[0x0009];
  _data.failedToStartShutdown = coils[0x0011];
  _data.highEngineTemperatureShutdown = coils[0x0012];
  _data.lowOilPressureShutdown = coils[0x0013];
  _data.lowFuelWarn = coils[0x001C];
  _data.chargeFailWarn = coils[0x001D];
  _data.batteryUndervoltageWarn = coils[0x001E];
  _data.batteryOvervoltageWarn = coils[0x001F];
  _data.autoMode = coils[0x0029];
  _data.manualMode = coils[0x002A];
  _data.stopMode = coils[0x002B];
  _data.online = true;
  return true;
}

const GensetData& Hgm6100nc::data() const {
  return _data;
}
