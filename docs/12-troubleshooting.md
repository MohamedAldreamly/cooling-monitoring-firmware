# Troubleshooting

| Symptom | Likely boundary | Checks |
|---|---|---|
| `API Online`, device offline | Device/status freshness | MQTT status topic, Devices timestamp, online timeout |
| Telemetry exists in DynamoDB but UI says unavailable | API or signal mapping | API response shape, `signals[]`, exact uppercase names, time range |
| Only one active alarm shown | Active-state reduction | Latest transition per alarm instance; closed events are history, not active |
| All signals become stale | Acquisition/simulation stopped | Sample timestamps, task health, stale thresholds, simulator loop |
| Alarm reaches broker but cursor does not move | Application ACK | ACK topic permission, payload `record_id`, `status: ok`, subscription |
| Alarm repeats after reboot | ACK/cursor persistence | Cursor file recovery, matching record ID, storage mount |
| White screen after Amplify deploy | Incorrect artifact root | `amplify.yml`, app root, `dist/index.html`, asset paths, SPA rewrite |
| Lambda `ResourceNotFoundException` | Wrong table/region/name | Lambda region and exact environment variable values |
| 404 for hashed JS/CSS after refresh | Stale or malformed deployment | Rebuild from source, deploy complete `dist`, invalidate cache |

## Firmware logs to expect

```text
Telemetry Publisher started: interval=10000 ms
Device Status Publisher started: interval=30000 ms
Wi-Fi connected; starting AWS IoT transport
```

## Fast isolation procedure

1. Confirm the ESP32 log is producing new snapshots.
2. Confirm the MQTT test client sees current payloads.
3. Confirm IoT Rule metrics show matched/successful actions.
4. Inspect the newest DynamoDB key and `received_at_ms`.
5. Call the API endpoint directly in the browser or curl.
6. Inspect the dashboard Network tab and compare JSON field names.

Do not debug all layers simultaneously; prove each boundary in order from device to UI.
