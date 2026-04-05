#pragma once
#include <Arduino.h>

class ModbusBus {
public:
  ModbusBus(HardwareSerial& serial, int deRePin);
  bool begin(uint32_t baudRate, int8_t rxPin, int8_t txPin);
  bool readHoldingRegisters(uint8_t slaveId, uint16_t startReg, uint16_t count, uint16_t* dest);
  bool readInputRegisters(uint8_t slaveId, uint16_t startReg, uint16_t count, uint16_t* dest);
  bool readCoils(uint8_t slaveId, uint16_t startReg, uint16_t count, bool* dest);
  String lastError() const;

private:
  static uint16_t crc16(const uint8_t* data, size_t len);
  bool transact(const uint8_t* request, size_t requestLen, uint8_t* response, size_t expectedLen);
  bool readRegisters(uint8_t slaveId, uint8_t functionCode, uint16_t startReg, uint16_t count, uint16_t* dest);

  HardwareSerial& _serial;
  int _deRePin;
  String _lastError;
};
