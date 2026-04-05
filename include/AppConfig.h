#pragma once
#include <Arduino.h>

namespace AppConfig {
  constexpr char PROJECT_NAME[] = "PowerEye AWS v1";
  constexpr char FW_VERSION[] = "0.1.0";

  constexpr uint32_t SERIAL_BAUD = 115200;

  constexpr uint32_t DEFAULT_REPORT_INTERVAL_MS = 60UL * 1000UL;
  constexpr uint32_t DEFAULT_RETRY_INTERVAL_MS  = 15UL * 1000UL;
  constexpr uint32_t DEFAULT_CONFIG_SYNC_MS     = 5UL * 60UL * 1000UL;

  constexpr uint32_t PZEM_POLL_INTERVAL_MS      = 5UL * 1000UL;
  constexpr uint32_t GENSET_POLL_INTERVAL_MS    = 5UL * 1000UL;
  constexpr uint32_t BATTERY_POLL_INTERVAL_MS   = 5UL * 1000UL;
  constexpr uint32_t FUEL_POLL_INTERVAL_MS      = 2UL * 1000UL;
  constexpr uint32_t DIGITAL_INPUT_POLL_MS      = 200UL;
  constexpr uint32_t MODEM_STATUS_POLL_MS       = 5UL * 1000UL;
  constexpr uint32_t MODBUS_RESPONSE_TIMEOUT_MS = 250UL;
  constexpr uint32_t MODBUS_INTERFRAME_DELAY_MS = 4UL;
  constexpr float    PZEM_ENERGY_WRAP_KWH       = 10000.0f;
  constexpr float    PZEM_WRAP_DETECT_HIGH_KWH  = 9800.0f;
  constexpr float    PZEM_WRAP_DETECT_LOW_KWH   = 300.0f;
  constexpr float    PZEM_RESET_DROP_MARGIN_KWH = 0.5f;
  constexpr uint32_t PZEM_ENERGY_PERSIST_MS     = 60UL * 1000UL;

  constexpr size_t   MAX_QUEUE_RECORDS          = 100;
  constexpr uint32_t DIGITAL_DEBOUNCE_MS        = 250UL;
  constexpr uint8_t  DEFAULT_AP_CHANNEL         = 6;
}
