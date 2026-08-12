# Cooling Monitoring Firmware

ESP-IDF firmware for ESP32-S3. The tested production pipeline is:

`Simulation -> Raw Queue -> Processing -> Snapshot -> Alarm -> Record -> SPIFFS`

## Raw-to-SPIFFS end-to-end test

The test is enabled by default:

`CONFIG_FIRMWARE_E2E_STORAGE_TEST=y`

No `durable_record_t` is created manually. The dedicated Simulation Engine
scenario emits normal `raw_sample_t` values into the same queue used by future
real drivers. All downstream components run unchanged.

The deterministic test timeline is:

- 0-3 seconds: `ROOM_TEMP_01 = -18 C` (normal baseline).
- 3-18 seconds: `ROOM_TEMP_01 = -5 C` (triggers `HIGH_ROOM_TEMP_01`).
- After 18 seconds: `ROOM_TEMP_01 = -18 C` (returns and closes the alarm).
- At 30 seconds: the observer prints PASS/FAIL for every pipeline stage and
  stops the simulator.

Expected final output:

```text
PIPELINE_E2E_TEST: RAW -> queue:          PASS
PIPELINE_E2E_TEST: Signal processing:     PASS
PIPELINE_E2E_TEST: Snapshot publishing:   PASS
PIPELINE_E2E_TEST: Alarm lifecycle:       PASS
PIPELINE_E2E_TEST: Record Builder:        PASS
PIPELINE_E2E_TEST: SPIFFS Storage Writer: PASS
PIPELINE_E2E_TEST: Queue/write drops:     PASS
PIPELINE_E2E_TEST: E2E TEST PASS: Raw -> Processing -> Snapshot -> Alarm -> Record -> SPIFFS
```

Build, flash and monitor:

```text
idf.py fullclean
idf.py reconfigure
idf.py build
idf.py -p COMx flash monitor
```

On the next reboot, `storage_manager_init()` reports the recovered journal
records before the new test begins. That confirms persistence across reset.

After completing the test, disable it using `idf.py menuconfig` under
`Cooling Monitoring Firmware`. Normal mode then uses the existing
`SIMULATION_SCENARIO_FULL_SYSTEM` until real drivers are integrated.
