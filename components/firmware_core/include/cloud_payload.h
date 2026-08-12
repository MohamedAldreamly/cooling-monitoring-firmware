#ifndef CLOUD_PAYLOAD_H
#define CLOUD_PAYLOAD_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "app_types.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Encodes a signal snapshot as a telemetry MQTT JSON payload.
 */
esp_err_t cloud_payload_encode_telemetry(
    const signal_snapshot_t *snapshot,
    uint32_t boot_id,
    char *buffer,
    size_t buffer_size
);

/**
 * @brief Encodes an alarm durable record as an MQTT JSON payload.
 */
esp_err_t cloud_payload_encode_alarm(
    const durable_record_t *record,
    char *buffer,
    size_t buffer_size
);

/**
 * @brief Encodes device status.
 */
esp_err_t cloud_payload_encode_status(
    uint32_t boot_id,
    uint64_t uptime_ms,
    bool simulation,
    const char *status,
    char *buffer,
    size_t buffer_size
);

#ifdef __cplusplus
}
#endif

#endif