# Testing Strategy

## Verification matrix

| Layer | Test | Pass condition |
|---|---|---|
| Signal registry | Startup validation | 16 unique, enabled definitions with valid ranges and timing |
| Simulation | Continuous alarm cycle | Signals enter and leave configured alarm states without ending the scenario |
| Storage | Power/restart recovery | Valid records remain readable and cursor is bounded by journal size |
| Offline replay | Wokwi Wi-Fi fault test | New alarms accumulate offline and upload in order after reconnect |
| ACK protocol | Wrong/malformed ACK injection | Cursor never advances |
| Telemetry | AWS MQTT client subscription | One full signal snapshot appears every 10 seconds while online |
| Status | Retained status subscription | Latest health payload is immediately available to a new subscriber |
| DynamoDB | Table queries | Items use the expected key types and timestamps |
| API | Route tests | Routes return expected status, CORS, and response schema |
| Dashboard | Build and smoke test | `npm run lint` and `npm run build` succeed; all routes render |

## Recommended end-to-end sequence

1. Clear or archive test table records if a clean run is required.
2. Subscribe to `cooling/cooling-unit-01/#` in the AWS IoT MQTT test client.
3. Start the Wokwi or physical device and verify startup logs.
4. Confirm telemetry, retained status, and alarm transitions.
5. Trigger the offline window and verify the journal backlog grows.
6. Confirm ordered replay and ACK-driven cursor advancement after reconnect.
7. Query DynamoDB and each HTTP endpoint.
8. Open the dashboard and compare displayed values with the newest telemetry item.

Do not use production data deletion as a routine test reset. Prefer dedicated development tables or TTL-based cleanup.
