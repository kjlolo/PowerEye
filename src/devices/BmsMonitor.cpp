#include "devices/BmsMonitor.h"

namespace {
constexpr uint16_t LIVE_BLOCK_START = 0x0000;
constexpr uint16_t LIVE_BLOCK_COUNT_CHANGHONG = 0x0037;
constexpr uint16_t LIVE_BLOCK_COUNT_WOLONG = 0x0042;

float signedTenths(int16_t raw) {
  return static_cast<float>(raw) / 10.0f;
}
}

BmsMonitor::BmsMonitor(ModbusBus& bus, uint8_t slaveId)
  : _bus(bus), _slaveId(slaveId) {}

void BmsMonitor::configure(BatteryModel model, uint8_t slaveId) {
  _model = model;
  _slaveId = slaveId;
}

void BmsMonitor::decodeCommon(const uint16_t* regs) {
  _data.packCurrent = static_cast<float>(static_cast<int16_t>(regs[0])) / 100.0f;
  _data.packVoltage = static_cast<float>(regs[1]) / 100.0f;
  _data.soc = static_cast<float>(regs[2] & 0x00FFU);
  _data.soh = static_cast<float>(regs[3] & 0x00FFU);
  _data.remainingCapacityAh = static_cast<float>(regs[4]) / 100.0f;
  _data.fullCapacityAh = static_cast<float>(regs[5]) / 100.0f;
  _data.designCapacityAh = static_cast<float>(regs[6]) / 100.0f;
  _data.cycleCount = regs[7];
  _data.warningFlags = regs[9];
  _data.protectionFlags = regs[10];
  _data.statusFlags = regs[11];
  _data.balanceStatus = regs[12];

  for (int i = 0; i < BatteryData::MAX_CELL_VOLTAGES; ++i) {
    _data.cellVoltagesMv[i] = static_cast<float>(regs[15 + i]);
  }
  for (int i = 0; i < BatteryData::MAX_CELL_TEMPERATURES; ++i) {
    _data.cellTemperaturesC[i] = signedTenths(static_cast<int16_t>(regs[31 + i]));
  }

  _data.mosfetTemperature = signedTenths(static_cast<int16_t>(regs[35]));
  _data.environmentTemperature = signedTenths(static_cast<int16_t>(regs[36]));
  _data.cellOvervoltageAlarm = (_data.warningFlags & (1U << 0)) != 0U;
  _data.cellUndervoltageAlarm = (_data.warningFlags & (1U << 1)) != 0U;
  _data.packOvervoltageAlarm = (_data.warningFlags & (1U << 2)) != 0U;
  _data.packUndervoltageAlarm = (_data.warningFlags & (1U << 3)) != 0U;
  _data.chargeOvercurrentAlarm = (_data.warningFlags & (1U << 4)) != 0U;
  _data.dischargeOvercurrentAlarm = (_data.warningFlags & (1U << 5)) != 0U;
  _data.lowSocAlarm = (_data.warningFlags & (1U << 15)) != 0U;
  _data.chargeMosfetOn = (_data.statusFlags & (1U << 10)) != 0U;
  _data.dischargeMosfetOn = (_data.statusFlags & (1U << 11)) != 0U;
  _data.heaterOn = (_data.statusFlags & (1U << 15)) != 0U;
}

bool BmsMonitor::poll() {
  if (_model == BatteryModel::NONE) {
    _data.online = false;
    return false;
  }

  const uint16_t readCount = (_model == BatteryModel::WOLONG) ? LIVE_BLOCK_COUNT_WOLONG : LIVE_BLOCK_COUNT_CHANGHONG;
  uint16_t regs[LIVE_BLOCK_COUNT_WOLONG] = {};
  if (!_bus.readHoldingRegisters(_slaveId, LIVE_BLOCK_START, readCount, regs)) {
    _data.online = false;
    return false;
  }

  decodeCommon(regs);
  _data.dischargeRemainingMinutes = 0.0f;
  _data.chargeRemainingMinutes = 0.0f;
  if (_model == BatteryModel::WOLONG) {
    _data.dischargeRemainingMinutes = static_cast<float>(regs[0x0040]);
    _data.chargeRemainingMinutes = static_cast<float>(regs[0x0041]);
  }

  _data.online = true;
  return true;
}

const BatteryData& BmsMonitor::data() const {
  return _data;
}
