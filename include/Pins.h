#pragma once

namespace Pins {
  // Air780e modem
  constexpr int MODEM_RX              = 17; // ESP32 TX <- Air780e RX
  constexpr int MODEM_TX              = 16; // ESP32 RX -> Air780e TX
  constexpr int MODEM_PWRKEY          = 18;

  // RS485 / MAX485
  constexpr int RS485_TX              = 21; // ESP32 TX -> MAX485 DI
  constexpr int RS485_DE_RE           = 22; // MAX485 DE/RE direction control
  constexpr int RS485_RX              = 23; // ESP32 RX <- MAX485 RO

  // Sensors
  constexpr int DHT22_DATA            = 19;
  constexpr int FUEL_ADC              = 32;
  constexpr int ANALOG_RESERVED       = 35;

  // Digital alarm inputs
  constexpr int DI_AC_MAINS_RECTIFIER = 34;
  constexpr int DI_GENSET_OPERATION   = 33;
  constexpr int DI_GENSET_FAILED      = 25;
  constexpr int DI_BATTERY_THEFT      = 26;
  constexpr int DI_POWER_CABLE_THEFT  = 27;
  constexpr int DI_RS_DOOR_OPEN       = 14;
  constexpr int DI_RESERVED           = 13;

  // Spare GPIO
  constexpr int GPIO_SPARE_18         = 4;
  constexpr int GPIO_SPARE_5          = 5;
}
