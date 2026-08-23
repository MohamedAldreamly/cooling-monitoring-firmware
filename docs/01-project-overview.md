# Project Overview

## Purpose

The platform gives operators remote, near-real-time visibility into a cold room and its refrigeration equipment while preserving alarm evidence during network outages.

## Primary actors

| Actor | Need |
|---|---|
| Operator | See temperatures, equipment state, active alarms, and device availability |
| Maintenance engineer | Diagnose trips, leaks, stale sensors, power loss, and storage backlog |
| Firmware engineer | Validate sampling, filtering, alarms, persistence, reconnect, and ACK behavior |
| Cloud engineer | Operate secure ingestion, DynamoDB storage, APIs, and dashboard delivery |

## Capabilities

1. Sixteen registered signals with type, unit, valid range, sample period, stale timeout, and filter configuration.
2. Local alarm transition generation with severity and source context.
3. Durable alarm journal and persistent upload cursor.
4. Live best-effort telemetry and retained device health.
5. Serverless, read-only dashboard API.
6. Responsive web UI and data-driven 3D plant representation.

## Non-goals in the current release

- Automatic control of compressors, fans, valves, or defrost cycles.
- Safety interlock replacement.
- Certified metrology or regulatory compliance claims.
- Multi-tenant identity and authorization.

## Quality attributes

| Attribute | Current design response |
|---|---|
| Connectivity resilience | Alarms are journaled and replayed after reconnect |
| Ordering | A single persistent cursor uploads stored alarm records sequentially |
| Observability | Status payload reports Wi-Fi, MQTT, storage, cursor, backlog, and cloud error counters |
| Freshness | Telemetry is not replayed; the newest snapshot replaces missed live samples |
| Safety | Dashboard and APIs are read-only; control is outside the release scope |
| Maintainability | Edge concerns are split into focused FreeRTOS components |
