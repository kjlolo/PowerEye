# PowerEye_AWS_v1

Starter PlatformIO project for Power Eye v1.

## Included in v1
- ESP32 starter structure
- Always-on Wi-Fi AP web UI
- Preferences-based config storage
- PZEM device class stub
- Fuel sensor class
- Digital inputs class
- Telemetry JSON builder
- Queue manager
- HTTP publish stub
- Air780e modem stub

## Still stubbed in this starter
- Actual Air780e AT command handling
- Actual HTTPS POST over modem
- Actual Modbus RTU transactions
- AWS remote config sync
- Genset/BMS/Rectifier drivers
- Authentication hardening for web UI

## Suggested next step
Port your known-good fuel logic and PZEM logic, then replace the modem and HTTP stubs with real Air780e command flow.


## Locked v1 hardware mapping
- GPIO32 — AC Mains Rectifier
- GPIO33 — Genset Operation
- GPIO25 — Genset Failed
- GPIO26 — Battery Theft
- GPIO27 — Power Cable Theft
- GPIO14 — RS Door Open
- GPIO13 — Reserved
- GPIO19 — DHT22
- GPIO34 — Fuel Sensor ADC
- GPIO35 — Reserved analog
- GPIO21/22/23 — RS485 TX / DE-RE / RX
- GPIO16/17/4 — Air780e RX / TX / PWRKEY
