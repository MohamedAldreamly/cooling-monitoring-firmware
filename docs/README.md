# Documentation Hub

This folder is the maintained technical source of truth for the Cooling Monitoring Platform. Documents describe the implementation on branch `dashboard-production`; historical requirement documents should be kept outside the active documentation set or clearly marked as archived.

## Reading paths

**New contributor:** [Overview](01-project-overview.md) → [Architecture](02-system-architecture.md) → [Firmware](03-firmware-architecture.md) → [Dashboard](09-dashboard-digital-twin.md)

**Cloud engineer:** [AWS architecture](05-aws-cloud-architecture.md) → [MQTT](06-mqtt-contract.md) → [Data model](07-data-model.md) → [API](08-api-reference.md)

**Operator / tester:** [Testing](10-testing.md) → [Deployment](11-deployment.md) → [Troubleshooting](12-troubleshooting.md)

| # | Document | Key question |
|---:|---|---|
| 01 | [Project overview](01-project-overview.md) | What is the platform and what is in scope? |
| 02 | [System architecture](02-system-architecture.md) | How do edge, cloud, and web components connect? |
| 03 | [Firmware architecture](03-firmware-architecture.md) | How does the ESP32 process, store, and upload data? |
| 04 | [Hardware architecture](04-hardware-architecture.md) | Which sensors and I/O channels exist? |
| 05 | [AWS cloud architecture](05-aws-cloud-architecture.md) | How is MQTT data ingested and served? |
| 06 | [MQTT contract](06-mqtt-contract.md) | Which topics, QoS levels, and payloads are used? |
| 07 | [Data model](07-data-model.md) | How are device, telemetry, and alarm records keyed? |
| 08 | [API reference](08-api-reference.md) | Which endpoints are consumed by the dashboard? |
| 09 | [Dashboard & digital twin](09-dashboard-digital-twin.md) | How does live data affect the UI and 3D plant? |
| 10 | [Testing](10-testing.md) | How is each layer verified? |
| 11 | [Deployment](11-deployment.md) | How are firmware and frontend released? |
| 12 | [Troubleshooting](12-troubleshooting.md) | Where should common failures be diagnosed? |

## Documentation principles

- Implementation beats aspiration: future work is labeled explicitly.
- Monitoring and control are kept separate; this release is read-only.
- Secrets, certificate private keys, account IDs, and personal data are never documented.
- MQTT names, API paths, and signal identifiers must match code exactly.
- Screenshots may show an offline device; that is a valid operating state, not an API failure.
