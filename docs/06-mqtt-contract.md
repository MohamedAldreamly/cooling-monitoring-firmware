# MQTT Contract

Device ID in the current firmware is `cooling-unit-01`; schema version is `1`.

| Topic | Direction | QoS | Retain | Purpose |
|---|---|---:|---:|---|
| `cooling/{deviceId}/telemetry` | device → cloud | 0 | no | Live 16-signal snapshot every 10 s |
| `cooling/{deviceId}/status` | device → cloud | 1 | yes | Device health every 30 s |
| `cooling/{deviceId}/alarm` | device → cloud | 1 | no | Durable alarm transition |
| `cooling/{deviceId}/alarm/ack` | cloud → device | 1 | no | Application acknowledgement |
| `$aws/things/{deviceId}/shadow/name/runtime/*` | both | per shadow operation | no | Named runtime shadow |

## Telemetry shape

```json
{
  "schema_version": 1,
  "message_type": "telemetry",
  "device_id": "cooling-unit-01",
  "snapshot_sequence": 146,
  "created_uptime_ms": 153583,
  "quality_summary": {"good": 16, "stale": 0, "fault": 0},
  "signals": [
    {"signal_id": 0, "name": "ROOM_TEMP_01", "value": -18.2, "unit": "degC", "quality": "good", "has_value": true}
  ]
}
```

## Status shape

Status includes firmware version, uptime, Wi-Fi/MQTT readiness, storage mount/degraded flags, journal size, cursor offset, last acknowledged record, pending bytes, disconnects, and error counters.

## Alarm ACK

```json
{"record_id":"42","status":"ok"}
```

`record_id` may be parsed from a JSON string or exact integer. Any mismatch, malformed payload, missing `ok` status, or unexpected ACK is rejected and does not advance the cursor.
