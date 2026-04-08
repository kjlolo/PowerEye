#include "devices/Hat600.h"

namespace {
constexpr uint16_t COIL_START = 0x0000;
constexpr uint16_t COIL_COUNT = 0x0030;
constexpr uint16_t REG_START = 0x0000;
constexpr uint16_t REG_COUNT = 0x0014;
// Function code 05 writable coils (Table 3)
constexpr uint16_t W_REMOTE_SOURCE1_CLOSE = 0x0000;
constexpr uint16_t W_REMOTE_OPEN = 0x0001;
constexpr uint16_t W_REMOTE_SOURCE2_CLOSE = 0x0002;
constexpr uint16_t W_MODE_AUTO_MANUAL = 0x0004;
constexpr uint16_t W_PRIORITY_SOURCE1 = 0x0005;
constexpr uint16_t W_PRIORITY_SOURCE2 = 0x0006;
constexpr uint16_t W_ALARM_RESET = 0x0007;
constexpr uint16_t W_REMOTE_START_GENERATOR = 0x0008;
constexpr uint16_t W_REMOTE_STOP_GENERATOR = 0x0009;

enum CoilIndex : uint16_t {
  SOURCE1_SWITCH_CLOSE = 0x0000,
  SOURCE2_SWITCH_CLOSE = 0x0001,
  SOURCE1_VOLTAGE_NORMAL = 0x0002,
  SOURCE2_VOLTAGE_NORMAL = 0x0003,
  AUTO_MODE = 0x0004,
  MANUAL_MODE = 0x0005,
  START_GENERATOR_OUTPUT = 0x0007,
  COMMON_WARNING = 0x0008,
  COMMON_ALARM = 0x0009,
  FAIL_TO_CHANGEOVER = 0x0010,
  SOURCE1_OVERVOLTAGE_ALARM = 0x0018,
  SOURCE1_UNDERVOLTAGE_ALARM = 0x0019,
  SOURCE1_OVERFREQ_ALARM = 0x001A,
  SOURCE1_UNDERFREQ_ALARM = 0x001B,
  SOURCE2_OVERVOLTAGE_ALARM = 0x001C,
  SOURCE2_UNDERVOLTAGE_ALARM = 0x001D,
  SOURCE2_OVERFREQ_ALARM = 0x001E,
  SOURCE2_UNDERFREQ_ALARM = 0x001F,
};

enum RegisterIndex : uint16_t {
  SRC1_VOLTAGE_A = 0x0000,
  SRC1_VOLTAGE_B = 0x0001,
  SRC1_VOLTAGE_C = 0x0002,
  SRC2_VOLTAGE_A = 0x0003,
  SRC2_VOLTAGE_B = 0x0004,
  SRC2_VOLTAGE_C = 0x0005,
  SRC1_CURRENT_A = 0x0006,
  SRC1_CURRENT_B = 0x0007,
  SRC1_CURRENT_C = 0x0008,
  SRC2_CURRENT_A = 0x0009,
  SRC2_CURRENT_B = 0x000A,
  SRC2_CURRENT_C = 0x000B,
  FREQ1_X10 = 0x000C,
  FREQ2_X10 = 0x000D,
  TOTAL_ACTIVE_POWER_X1000 = 0x000E,
  TOTAL_APPARENT_POWER_X1000 = 0x000F,
  TOTAL_POWER_FACTOR_X1000 = 0x0010,
};
}

Hat600::Hat600(ModbusBus& bus, uint8_t slaveId)
  : _bus(bus), _slaveId(slaveId) {}

void Hat600::setSlaveId(uint8_t slaveId) {
  _slaveId = slaveId;
}

bool Hat600::setMode(Mode mode) {
  const bool autoMode = (mode == Mode::AUTO);
  const bool ok = _bus.writeSingleCoil(_slaveId, W_MODE_AUTO_MANUAL, autoMode);
  _lastError = ok ? "" : _bus.lastError();
  return ok;
}

bool Hat600::switchTo(SwitchTarget target) {
  bool ok = false;
  switch (target) {
    case SwitchTarget::SOURCE1:
      ok = _bus.writeSingleCoil(_slaveId, W_REMOTE_SOURCE1_CLOSE, true);
      break;
    case SwitchTarget::SOURCE2:
      ok = _bus.writeSingleCoil(_slaveId, W_REMOTE_SOURCE2_CLOSE, true);
      break;
    case SwitchTarget::OPEN_BOTH:
      ok = _bus.writeSingleCoil(_slaveId, W_REMOTE_OPEN, true);
      break;
    default:
      _lastError = "invalid_target";
      return false;
  }
  _lastError = ok ? "" : _bus.lastError();
  return ok;
}

bool Hat600::setPrioritySource1() {
  const bool ok = _bus.writeSingleCoil(_slaveId, W_PRIORITY_SOURCE1, true);
  _lastError = ok ? "" : _bus.lastError();
  return ok;
}

bool Hat600::setPrioritySource2() {
  const bool ok = _bus.writeSingleCoil(_slaveId, W_PRIORITY_SOURCE2, true);
  _lastError = ok ? "" : _bus.lastError();
  return ok;
}

bool Hat600::resetAlarm() {
  const bool ok = _bus.writeSingleCoil(_slaveId, W_ALARM_RESET, true);
  _lastError = ok ? "" : _bus.lastError();
  return ok;
}

bool Hat600::remoteStartGenerator() {
  const bool ok = _bus.writeSingleCoil(_slaveId, W_REMOTE_START_GENERATOR, true);
  _lastError = ok ? "" : _bus.lastError();
  return ok;
}

bool Hat600::remoteStopGenerator() {
  const bool ok = _bus.writeSingleCoil(_slaveId, W_REMOTE_STOP_GENERATOR, true);
  _lastError = ok ? "" : _bus.lastError();
  return ok;
}

String Hat600::lastError() const {
  return _lastError;
}

bool Hat600::poll() {
  uint16_t regs[REG_COUNT] = {};
  bool coils[COIL_COUNT] = {};

  if (!_bus.readHoldingRegisters(_slaveId, REG_START, REG_COUNT, regs)) {
    _data.online = false;
    return false;
  }
  if (!_bus.readCoils(_slaveId, COIL_START, COIL_COUNT, coils)) {
    _data.online = false;
    return false;
  }

  _data.source1SwitchClosed = coils[SOURCE1_SWITCH_CLOSE];
  _data.source2SwitchClosed = coils[SOURCE2_SWITCH_CLOSE];
  _data.source1VoltageNormal = coils[SOURCE1_VOLTAGE_NORMAL];
  _data.source2VoltageNormal = coils[SOURCE2_VOLTAGE_NORMAL];
  _data.autoMode = coils[AUTO_MODE];
  _data.manualMode = coils[MANUAL_MODE];
  _data.startGeneratorOutput = coils[START_GENERATOR_OUTPUT];
  _data.commonWarning = coils[COMMON_WARNING];
  _data.commonAlarm = coils[COMMON_ALARM];
  _data.failToChangeover = coils[FAIL_TO_CHANGEOVER];
  _data.source1OvervoltageAlarm = coils[SOURCE1_OVERVOLTAGE_ALARM];
  _data.source1UndervoltageAlarm = coils[SOURCE1_UNDERVOLTAGE_ALARM];
  _data.source1OverfrequencyAlarm = coils[SOURCE1_OVERFREQ_ALARM];
  _data.source1UnderfrequencyAlarm = coils[SOURCE1_UNDERFREQ_ALARM];
  _data.source2OvervoltageAlarm = coils[SOURCE2_OVERVOLTAGE_ALARM];
  _data.source2UndervoltageAlarm = coils[SOURCE2_UNDERVOLTAGE_ALARM];
  _data.source2OverfrequencyAlarm = coils[SOURCE2_OVERFREQ_ALARM];
  _data.source2UnderfrequencyAlarm = coils[SOURCE2_UNDERFREQ_ALARM];

  _data.source1VoltageA = static_cast<float>(regs[SRC1_VOLTAGE_A]);
  _data.source1VoltageB = static_cast<float>(regs[SRC1_VOLTAGE_B]);
  _data.source1VoltageC = static_cast<float>(regs[SRC1_VOLTAGE_C]);
  _data.source2VoltageA = static_cast<float>(regs[SRC2_VOLTAGE_A]);
  _data.source2VoltageB = static_cast<float>(regs[SRC2_VOLTAGE_B]);
  _data.source2VoltageC = static_cast<float>(regs[SRC2_VOLTAGE_C]);
  _data.source1CurrentA = static_cast<float>(regs[SRC1_CURRENT_A]);
  _data.source1CurrentB = static_cast<float>(regs[SRC1_CURRENT_B]);
  _data.source1CurrentC = static_cast<float>(regs[SRC1_CURRENT_C]);
  _data.source2CurrentA = static_cast<float>(regs[SRC2_CURRENT_A]);
  _data.source2CurrentB = static_cast<float>(regs[SRC2_CURRENT_B]);
  _data.source2CurrentC = static_cast<float>(regs[SRC2_CURRENT_C]);
  _data.frequency1 = static_cast<float>(regs[FREQ1_X10]) / 10.0f;
  _data.frequency2 = static_cast<float>(regs[FREQ2_X10]) / 10.0f;
  _data.totalActivePowerKw = static_cast<float>(regs[TOTAL_ACTIVE_POWER_X1000]) / 1000.0f;
  _data.totalApparentPowerKva = static_cast<float>(regs[TOTAL_APPARENT_POWER_X1000]) / 1000.0f;
  _data.totalPowerFactor = static_cast<float>(regs[TOTAL_POWER_FACTOR_X1000]) / 1000.0f;

  _data.online = true;
  return true;
}

const AtsData& Hat600::data() const {
  return _data;
}
