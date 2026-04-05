# Power Eye Modbus Register Reference

This document consolidates the Modbus register tables extracted from the current protocol PDFs so the firmware can implement model-selectable drivers without re-reading the manuals.

## Source PDFs

- `PZEM-016(1).pdf`
- `HGM6100NC_Procotol_en (1)(1).pdf`
- `Changhong Modbus-BMS-protocol V1.3(1).pdf`
- `WOLONG Modbus-bms-protocol V1.5(2017-11-02)_fc6d46df-f5d6-414c-9939-61f1975b04b7.pdf`

## Common Modbus Notes

- `PZEM-016`
  - RS485, `9600 8N1`
  - Read measurements with function `0x04`
  - Read parameters with function `0x03`
  - Write single register with function `0x06`
- `HGM6100NC`
  - Uses Modbus RTU
  - Coil status via function `0x01`
  - Holding registers via function `0x03`
  - Remote control coils via function `0x05`
- `Changhong BMS` and `Wolong BMS`
  - Default `9600 8N1`
  - Read registers with function `0x03`
  - Write multiple registers with function `0x10`
  - Slave IDs `0x01` to `0x10`

## Recommended Poll Sets

These are the minimal read groups worth implementing first.

- `PZEM-016`
  - Read `0x0000` to `0x0009` with function `0x04`
- `HGM6100NC`
  - Read `0x0007` to `0x001D` for generator electrical and engine values
  - Read `0x0022` to `0x0032` for state, runtime, starts, and energy counters
  - Read coils `0x0000` to `0x0047` with function `0x01` for alarms and state flags
- `Changhong BMS`
  - Read `0000` to `0036` for live telemetry and flags
- `Wolong BMS`
  - Read `0000` to `0041` for live telemetry, flags, and remaining time values

## PZEM-016

### Measurement Registers

Use function `0x04` starting at `0x0000`.

| Address | Description | Raw scaling |
| --- | --- | --- |
| `0x0000` | Voltage | `0.1 V / LSB` |
| `0x0001` | Current low 16 bits | `0.001 A / LSB` |
| `0x0002` | Current high 16 bits | combine with `0x0001` |
| `0x0003` | Power low 16 bits | `0.1 W / LSB` |
| `0x0004` | Power high 16 bits | combine with `0x0003` |
| `0x0005` | Energy low 16 bits | `1 Wh / LSB` |
| `0x0006` | Energy high 16 bits | combine with `0x0005` |
| `0x0007` | Frequency | `0.1 Hz / LSB` |
| `0x0008` | Power factor | `0.01 / LSB` |
| `0x0009` | Alarm status | `0xFFFF = alarm`, `0x0000 = normal` |

### Parameter Registers

Use function `0x03` to read and function `0x06` to write.

| Address | Description | Raw scaling |
| --- | --- | --- |
| `0x0001` | Power alarm threshold | `1 W / LSB` |
| `0x0002` | Modbus address | valid range `0x0001` to `0x00F7` |

### Implementation Notes

- Current, power, and energy are 32-bit values split into low and high 16-bit registers.
- The manual example reads `0x0000` through `0x0009` in one request.

## HGM6100NC Generator Controller

### Function `0x03` Register Map

| Address | Description | Raw scaling |
| --- | --- | --- |
| `0000H` | Mains phase A voltage LN | `1 V / LSB` |
| `0001H` | Mains phase B voltage LN | `1 V / LSB` |
| `0002H` | Mains phase C voltage LN | `1 V / LSB` |
| `0003H` | Mains phase AB voltage LL | `1 V / LSB` |
| `0004H` | Mains phase BC voltage LL | `1 V / LSB` |
| `0005H` | Mains phase CA voltage LL | `1 V / LSB` |
| `0006H` | Mains frequency | `0.1 Hz / LSB` |
| `0007H` | Generator phase A voltage LN | `1 V / LSB` |
| `0008H` | Generator phase B voltage LN | `1 V / LSB` |
| `0009H` | Generator phase C voltage LN | `1 V / LSB` |
| `000AH` | Generator phase AB voltage LL | `1 V / LSB` |
| `000BH` | Generator phase BC voltage LL | `1 V / LSB` |
| `000CH` | Generator phase CA voltage LL | `1 V / LSB` |
| `000DH` | Generator frequency | `0.1 Hz / LSB` |
| `000EH` | Phase A current | `0.1 A / LSB` |
| `000FH` | Phase B current | `0.1 A / LSB` |
| `0010H` | Phase C current | `0.1 A / LSB` |
| `0011H` | Engine temperature | `1 C / LSB` |
| `0012H` | Temperature sender resistor | raw |
| `0013H` | Oil pressure | `1 kPa / LSB` |
| `0014H` | Pressure sender resistor | raw |
| `0015H` | Fuel level | `1 % / LSB` |
| `0016H` | Level sender resistor | raw |
| `0017H` | Engine speed | `1 RPM / LSB` |
| `0018H` | Battery voltage | `0.1 V / LSB` |
| `0019H` | Charger D+ voltage | `0.1 V / LSB` |
| `001AH` | Total active power | `0.1 kW / LSB` |
| `001BH` | Total reactive power | `0.1 kVar / LSB` |
| `001CH` | Total apparent power | `0.1 kVA / LSB` |
| `001DH` | Power factor | `0.01 / LSB` |
| `0022H` | Generator state | enum/raw |
| `0023H` | Generator delay | raw |
| `0024H` | Remote start state | raw |
| `0025H` | Remote start delay | raw |
| `0026H` | ATS state | raw |
| `0027H` | ATS delay | raw |
| `0028H` | Mains state | raw |
| `0029H` | Mains delay | raw |
| `002AH` | Hours of run high | combine with `002BH` if needed |
| `002BH` | Hours of run low | combine or store separately |
| `002CH` | Minutes of run | `0-59` |
| `002DH` | Seconds of run | `0-59` |
| `002EH` | Number of starts high | combine with `002FH` if needed |
| `002FH` | Number of starts low | combine or store separately |
| `0030H` | Total energy high | combine with `0031H` if needed |
| `0031H` | Total energy low | combine or store separately |
| `0032H` | Software version | raw |

### Function `0x01` Coil Status Map

These are the most useful coils for telemetry and alarms.

| Address | Meaning |
| --- | --- |
| `0000H` | Common alarm |
| `0001H` | Common warn |
| `0002H` | Common shutdown |
| `0005H` | Generators normal |
| `0006H` | Mains load |
| `0007H` | Generators load |
| `0008H` | Emergency stop shutdown |
| `0009H` | Over speed shutdown |
| `000AH` | Under speed shutdown |
| `000CH` | Over frequency shutdown |
| `000DH` | Under frequency shutdown |
| `000EH` | Over generator voltage shutdown |
| `000FH` | Under generator voltage shutdown |
| `0010H` | Over current shutdown |
| `0011H` | Failed to start shutdown |
| `0012H` | High engine temperature shutdown |
| `0013H` | Low oil pressure shutdown |
| `0018H` | High engine temperature warn |
| `0019H` | Low oil pressure warn |
| `001AH` | Over current warn |
| `001BH` | Failed to stop warn |
| `001CH` | Low fuel level warn |
| `001DH` | Charge fail warn |
| `001EH` | Under battery voltage warn |
| `001FH` | Over battery voltage warn |
| `0020H` | Input warn |
| `0028H` | System at test mode |
| `0029H` | System at auto mode |
| `002AH` | System at manual mode |
| `002BH` | System at stop mode |
| `0030H` | Emergency stop input closed |
| `0031H` | Input 1 closed |
| `0032H` | Input 2 closed |
| `0033H` | Input 3 closed |
| `0034H` | Input 4 closed |
| `0035H` | Input 5 closed |
| `0038H` | Start relay output |
| `0039H` | Fuel relay output |
| `003AH` | Config output 1 |
| `003BH` | Config output 2 |
| `003CH` | Config output 3 |
| `003DH` | Config output 4 |
| `0040H` | Mains abnormal |
| `0041H` | Mains normal |
| `0042H` | Mains over voltage |
| `0043H` | Mains under voltage |
| `0044H` | Mains lost phase |

### Function `0x05` Remote Coils

| Address | Meaning |
| --- | --- |
| `0000H` | Start generator |
| `0001H` | Stop generator |
| `0002H` | Set test mode |
| `0003H` | Set automatic mode |
| `0004H` | Set manual mode |

## Changhong BMS

### Live Telemetry Registers

Use function `0x03`.

| Address | Description | Type | Scaling / notes |
| --- | --- | --- | --- |
| `0000` | Current | `INT16` | `10 mA / LSB`, positive charging, negative discharging |
| `0001` | Pack voltage | `UINT16` | `10 mV / LSB` |
| `0002` | SOC | `UINT8` | `%` |
| `0003` | SOH | `UINT8` | `%` |
| `0004` | Remaining capacity | `UINT16` | `10 mAh / LSB` |
| `0005` | Full capacity | `UINT16` | `10 mAh / LSB` |
| `0006` | Design capacity | `UINT16` | `10 mAh / LSB` |
| `0007` | Cycle count | `UINT16` | cycles |
| `0009` | Warning flag | `UINT16` | bitfield |
| `0010` | Protection flag | `UINT16` | bitfield |
| `0011` | Status/fault flag | `UINT16` | bitfield |
| `0012` | Balance status | `UINT16` | bitfield/raw |
| `0015-0030` | Cell voltages 1-16 | `UINT16 x16` | `mV` |
| `0031-0034` | Cell temperatures 1-4 | `INT16 x4` | `0.1 C / LSB` |
| `0035` | MOSFET temperature | `INT16` | `0.1 C / LSB`, may be invalid on some models |
| `0036` | Environment temperature | `INT16` | `0.1 C / LSB`, may be invalid on some models |

### Identification Registers

| Address | Description | Notes |
| --- | --- | --- |
| `0150-0159` | Version information | ASCII in `UINT16` registers |
| `0160-0169` | Model SN | ASCII, manufacturer side |
| `0170-0179` | PACK SN | ASCII, pack side |

### Warning Flag `0009` Bits

| Bit | Meaning |
| --- | --- |
| `BIT0` | Cell overvoltage alarm |
| `BIT1` | Cell undervoltage alarm |
| `BIT2` | Pack overvoltage alarm |
| `BIT3` | Pack undervoltage alarm |
| `BIT4` | Charging overcurrent alarm |
| `BIT5` | Discharging overcurrent alarm |
| `BIT8` | Charging high temperature alarm |
| `BIT9` | Discharging high temperature alarm |
| `BIT10` | Charging low temperature alarm |
| `BIT11` | Discharging low temperature alarm |
| `BIT12` | Environment high temperature alarm |
| `BIT13` | Environment low temperature alarm |
| `BIT14` | MOSFET high temperature alarm |
| `BIT15` | Low SOC alarm |

### Protection Flag `0010` Bits

| Bit | Meaning |
| --- | --- |
| `BIT0` | Cell overvoltage protection |
| `BIT1` | Cell undervoltage protection |
| `BIT2` | Pack overvoltage protection |
| `BIT3` | Pack undervoltage protection |
| `BIT4` | Charging overcurrent protection |
| `BIT5` | Discharging overcurrent protection |
| `BIT6` | Short circuit protection |
| `BIT7` | Charger overvoltage protection |
| `BIT8` | Charging high temperature protection |
| `BIT9` | Discharging high temperature protection |
| `BIT10` | Charging low temperature protection |
| `BIT11` | Discharging low temperature protection |
| `BIT12` | MOSFET high temperature protection |
| `BIT13` | Environment high temperature protection |
| `BIT14` | Environment low temperature protection |

### Status/Fault Flag `0011` Bits

| Bit | Meaning |
| --- | --- |
| `BIT0` | Charging MOSFET fault |
| `BIT1` | Discharging MOSFET fault |
| `BIT2` | Temperature sensor fault |
| `BIT4` | Battery cell fault |
| `BIT5` | Front-end sampling communication fault |
| `BIT8` | State of charge active |
| `BIT9` | State of discharge active |
| `BIT10` | Charging MOSFET ON |
| `BIT11` | Discharging MOSFET ON |
| `BIT12` | Charging limiter ON |
| `BIT14` | Charger inversed |
| `BIT15` | Heater ON |

## Wolong BMS

Wolong uses the same live telemetry and flag layout as the Changhong map in the extracted pages, with additional readable runtime values and a longer PACK SN range.

### Live Telemetry Registers

| Address | Description | Type | Scaling / notes |
| --- | --- | --- | --- |
| `0000` | Current | `INT16` | `10 mA / LSB`, positive charging, negative discharging |
| `0001` | Pack voltage | `UINT16` | `10 mV / LSB` |
| `0002` | SOC | `UINT8` | `%` |
| `0003` | SOH | `UINT8` | `%` |
| `0004` | Remaining capacity | `UINT16` | `10 mAh / LSB` |
| `0005` | Full capacity | `UINT16` | `10 mAh / LSB` |
| `0006` | Design capacity | `UINT16` | `10 mAh / LSB` |
| `0007` | Cycle count | `UINT16` | cycles |
| `0009` | Warning flag | `UINT16` | bitfield |
| `0010` | Protection flag | `UINT16` | bitfield |
| `0011` | Status/fault flag | `UINT16` | bitfield |
| `0012` | Balance status | `UINT16` | bitfield/raw |
| `0015-0030` | Cell voltages 1-16 | `UINT16 x16` | `mV` |
| `0031-0034` | Cell temperatures 1-4 | `INT16 x4` | `0.1 C / LSB` |
| `0035` | MOSFET temperature | `INT16` | `0.1 C / LSB`, may be invalid on some models |
| `0036` | Environment temperature | `INT16` | `0.1 C / LSB`, may be invalid on some models |
| `0040` | Discharging remaining time | `UINT16` | minutes |
| `0041` | Charging remaining time | `UINT16` | minutes |

### Extra Registers

| Address | Description | Notes |
| --- | --- | --- |
| `0059` | Sleep control | write `0x0055` to sleep |
| `0150-0159` | Version information | ASCII in `UINT16` registers |
| `0160-0169` | Model SN | ASCII, manufacturer side |
| `0170-0184` | PACK SN | ASCII, pack side |

### Flag Bits

Use the same `0009`, `0010`, and `0011` bit meanings as `Changhong BMS`.

## Firmware Mapping Guidance

These tables are enough for a first implementation of model-selectable drivers.

- `Pzem016Driver`
  - Single read block `0x0000-0x0009`
  - Build voltage, current, power, energy, frequency, PF, alarm
- `Hgm6100ncDriver`
  - Register block `0x0007-0x001D`
  - Register block `0x0022-0x0032`
  - Coil block `0x0000-0x0047`
  - Optional write coils `0x0000-0x0004` for remote actions
- `ChanghongBmsDriver`
  - Register block `0000-0036`
  - Optional ID block `0150-0179`
- `WolongBmsDriver`
  - Register block `0000-0041`
  - Optional ID block `0150-0184`

## Known Differences To Preserve In Code

- `Changhong PACK SN` uses `0170-0179`.
- `Wolong PACK SN` uses `0170-0184`.
- `Wolong` exposes `0040` and `0041` for remaining time; `Changhong` extracted map does not.
- `MOSFET` and `Environment temperature` may be invalid on some BMS variants.
- Some manuals mix `1-byte` field descriptions with 16-bit register storage. The implementation should still read whole 16-bit registers and mask or cast as needed.
