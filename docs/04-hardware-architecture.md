# Hardware Architecture

## Signal map

| ID | Signal | Type | Unit | Source | Channel | Sample | Stale after |
|---:|---|---|---|---|---:|---:|---:|
| 0–3 | `ROOM_TEMP_01..04` | float | °C | ADAM-4115 | 0–3 | 2 s | 15 s |
| 4 | `AIR_TEMP_01` | float | °C | Vaisala HMP110 | 0 | 2 s | 15 s |
| 5 | `AIR_RH_01` | float | %RH | Vaisala HMP110 | 1 | 2 s | 15 s |
| 6 | `DOOR_SAFE` | bool | — | ADAM-4051 | 0 | 100 ms | 1 s |
| 7 | `DOOR_AUX` | bool | — | ADAM-4051 | 1 | 100 ms | 1 s |
| 8 | `LEAK_ALARM` | bool | — | ADAM-4051 | 2 | 100 ms | 1 s |
| 9 | `LEAK_CABLE_FAULT` | bool | — | ADAM-4051 | 3 | 100 ms | 1 s |
| 10 | `COMPRESSOR_RUN` | bool | — | ADAM-4051 | 4 | 100 ms | 1 s |
| 11 | `COMPRESSOR_TRIP` | bool | — | ADAM-4051 | 5 | 100 ms | 1 s |
| 12 | `EVAP_FAN_RUN` | bool | — | ADAM-4051 | 6 | 100 ms | 1 s |
| 13 | `POWER_FAILURE` | bool | — | ADAM-4051 | 7 | 100 ms | 1 s |
| 14 | `COMPRESSOR_CURRENT` | float | A | ADAM-4017 | 0 | 1 s | 5 s |
| 15 | `BATTERY_VOLTAGE` | float | V | ADAM-4017 | 1 | 5 s | 20 s |

## Buses and modules

- ADAM field bus: RTU devices at addresses 1 (ADAM-4115), 2 (ADAM-4051), and 3 (ADAM-4017).
- Vaisala bus: HMP110 at address 240.
- Analog temperatures and electrical measurements use moving-average filters.
- Digital safety states are unfiltered and sampled at 100 ms.

## Commissioning note

The current registry uses `MODBUS_REGISTER_UNDEFINED` placeholders. Real register addresses, electrical isolation, power supply, termination, shield grounding, fail-safe polarity, and cable routing must be finalized and verified against manufacturer manuals before hardware commissioning.
