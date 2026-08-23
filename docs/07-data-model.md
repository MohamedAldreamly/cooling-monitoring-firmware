# Data Model

## DynamoDB tables

| Table | Partition key | Sort key | Use |
|---|---|---|---|
| `Devices` | `device_id` (S) | — | Latest device health/status |
| `Telemetry` | `device_id` (S) | `received_at_ms` (N) | Time-ordered snapshots |
| `CoolingAlarmRecords` | `device_id` (S) | `record_id` (N) | Alarm transition history |

## Telemetry item

Each item keeps the complete `signals` list as nested DynamoDB maps, plus ingestion time, firmware version, schema version, snapshot sequence, uptime, and quality summary. Querying by device and time range uses the primary key without a table scan.

## Alarm item

Alarm history stores transition-level records. Multiple rows can belong to the same logical alarm instance. Typical fields include `alarm_instance_id`, `alarm_code`, `severity`, `transition`, `transition_sequence`, `source_signal`, `source_quality`, and `source_value`.

## Active alarm projection

An active alarm is a derived state, not every row whose severity is high. The API groups history by alarm identity and evaluates the latest transition. Returned/closed transitions remove that instance from the active set.

## Time semantics

- `received_at_ms`: cloud ingestion timestamp used for DynamoDB ordering and dashboard queries.
- `created_uptime_ms` / `uptime_ms`: monotonic device time since boot.
- `observed_at_ms`: nullable until the device has a trusted wall clock.
