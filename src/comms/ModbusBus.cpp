#include "comms/ModbusBus.h"
#include "AppConfig.h"

ModbusBus::ModbusBus(HardwareSerial& serial, int deRePin)
  : _serial(serial), _deRePin(deRePin) {}

bool ModbusBus::begin(uint32_t baudRate, int8_t rxPin, int8_t txPin) {
  pinMode(_deRePin, OUTPUT);
  digitalWrite(_deRePin, LOW);
  _serial.begin(baudRate, SERIAL_8N1, rxPin, txPin);
  return true;
}

uint16_t ModbusBus::crc16(const uint8_t* data, size_t len) {
  uint16_t crc = 0xFFFF;
  for (size_t pos = 0; pos < len; ++pos) {
    crc ^= data[pos];
    for (uint8_t i = 0; i < 8; ++i) {
      if ((crc & 0x0001U) != 0U) {
        crc >>= 1;
        crc ^= 0xA001U;
      } else {
        crc >>= 1;
      }
    }
  }
  return crc;
}

bool ModbusBus::transact(const uint8_t* request, size_t requestLen, uint8_t* response, size_t expectedLen) {
  _lastError = "";

  while (_serial.available() > 0) {
    _serial.read();
  }

  digitalWrite(_deRePin, HIGH);
  delay(AppConfig::MODBUS_INTERFRAME_DELAY_MS);
  _serial.write(request, requestLen);
  _serial.flush();
  digitalWrite(_deRePin, LOW);

  const unsigned long start = millis();
  size_t offset = 0;
  while ((millis() - start) < AppConfig::MODBUS_RESPONSE_TIMEOUT_MS && offset < expectedLen) {
    while (_serial.available() > 0 && offset < expectedLen) {
      response[offset++] = static_cast<uint8_t>(_serial.read());
    }
    delay(1);
  }

  if (offset != expectedLen) {
    _lastError = "modbus_timeout";
    return false;
  }

  const uint16_t actualCrc = crc16(response, expectedLen - 2);
  const uint16_t responseCrc = static_cast<uint16_t>(response[expectedLen - 2]) |
                               (static_cast<uint16_t>(response[expectedLen - 1]) << 8);
  if (actualCrc != responseCrc) {
    _lastError = "modbus_crc";
    return false;
  }

  if ((response[1] & 0x80U) != 0U) {
    _lastError = "modbus_exception_" + String(response[2]);
    return false;
  }

  return true;
}

bool ModbusBus::readRegisters(uint8_t slaveId, uint8_t functionCode, uint16_t startReg, uint16_t count, uint16_t* dest) {
  if (count == 0 || dest == nullptr) {
    _lastError = "bad_args";
    return false;
  }

  uint8_t request[8] = {
    slaveId,
    functionCode,
    static_cast<uint8_t>(startReg >> 8),
    static_cast<uint8_t>(startReg & 0xFF),
    static_cast<uint8_t>(count >> 8),
    static_cast<uint8_t>(count & 0xFF),
    0,
    0
  };
  const uint16_t reqCrc = crc16(request, 6);
  request[6] = static_cast<uint8_t>(reqCrc & 0xFF);
  request[7] = static_cast<uint8_t>((reqCrc >> 8) & 0xFF);

  const size_t expectedLen = 5U + (count * 2U);
  uint8_t response[256] = {};
  if (expectedLen > sizeof(response)) {
    _lastError = "response_too_large";
    return false;
  }
  if (!transact(request, sizeof(request), response, expectedLen)) {
    return false;
  }

  if (response[0] != slaveId || response[1] != functionCode || response[2] != count * 2U) {
    _lastError = "bad_response";
    return false;
  }

  for (uint16_t i = 0; i < count; ++i) {
    dest[i] = static_cast<uint16_t>(response[3 + (i * 2U)] << 8) | response[4 + (i * 2U)];
  }
  return true;
}

bool ModbusBus::readHoldingRegisters(uint8_t slaveId, uint16_t startReg, uint16_t count, uint16_t* dest) {
  return readRegisters(slaveId, 0x03, startReg, count, dest);
}

bool ModbusBus::readInputRegisters(uint8_t slaveId, uint16_t startReg, uint16_t count, uint16_t* dest) {
  return readRegisters(slaveId, 0x04, startReg, count, dest);
}

bool ModbusBus::readCoils(uint8_t slaveId, uint16_t startReg, uint16_t count, bool* dest) {
  if (count == 0 || dest == nullptr) {
    _lastError = "bad_args";
    return false;
  }

  uint8_t request[8] = {
    slaveId,
    0x01,
    static_cast<uint8_t>(startReg >> 8),
    static_cast<uint8_t>(startReg & 0xFF),
    static_cast<uint8_t>(count >> 8),
    static_cast<uint8_t>(count & 0xFF),
    0,
    0
  };
  const uint16_t reqCrc = crc16(request, 6);
  request[6] = static_cast<uint8_t>(reqCrc & 0xFF);
  request[7] = static_cast<uint8_t>((reqCrc >> 8) & 0xFF);

  const uint8_t byteCount = static_cast<uint8_t>((count + 7U) / 8U);
  const size_t expectedLen = 5U + byteCount;
  uint8_t response[256] = {};
  if (!transact(request, sizeof(request), response, expectedLen)) {
    return false;
  }

  if (response[0] != slaveId || response[1] != 0x01 || response[2] != byteCount) {
    _lastError = "bad_response";
    return false;
  }

  for (uint16_t i = 0; i < count; ++i) {
    const uint8_t packed = response[3 + (i / 8U)];
    dest[i] = ((packed >> (i % 8U)) & 0x01U) != 0U;
  }
  return true;
}

String ModbusBus::lastError() const {
  return _lastError;
}
