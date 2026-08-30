# HTTP API Reference

The dashboard consumes a read-only HTTP API. All JSON responses include CORS headers. Production deployments should replace wildcard CORS with the Amplify origin and add authentication before exposing sensitive operational data.

| Method | Route | Purpose |
|---|---|---|
| GET | `/dashboard/summary` | Device and active-alarm counts |
| GET | `/devices` | Sorted device list |
| GET | `/devices/{deviceId}` | Latest status for one device |
| GET | `/devices/{deviceId}/telemetry` | Time-ordered telemetry query |
| GET | `/devices/{deviceId}/alarms` | Alarm transition history |
| GET | `/alarms/active` | Current unresolved alarms |

## Telemetry query parameters

| Parameter | Type | Default | Constraint |
|---|---|---:|---|
| `limit` | integer | 100 | 1–500 |
| `from` | Unix ms | — | inclusive lower timestamp |
| `to` | Unix ms | — | inclusive upper timestamp |

Newest records are returned first. `from` and `to` must be numeric Unix timestamps in milliseconds.

## Example summary

```json
{
  "total_devices": 1,
  "online_devices": 1,
  "offline_devices": 0,
  "active_alarms": 2,
  "critical_alarms": 1
}
```

## Error contract

Expected status codes are `400` for invalid parameters, `404` for unknown routes/devices, and `500` for unhandled cloud errors. Errors should include a stable `error` string without leaking AWS account details or stack traces.
