#ifndef TELEMETRY_PAYLOAD_H
#define TELEMETRY_PAYLOAD_H

#include <stddef.h>

#include "app_types.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Converts one signal snapshot into Telemetry JSON.
 *
 * Telemetry is live data and is not converted into a durable_record_t.
 * It is not stored in journal.bin and does not use the upload cursor.
 *
 * @param snapshot Snapshot containing the latest signal values.
 * @param output Buffer where the generated JSON will be written.
 * @param output_size Total size of the output buffer.
 *
 * @return ESP_OK when JSON is generated successfully.
 * @return ESP_ERR_INVALID_ARG when an argument is invalid.
 * @return ESP_ERR_NO_MEM when JSON objects cannot be allocated.
 * @return ESP_ERR_INVALID_SIZE when the output buffer is too small.
 */
esp_err_t telemetry_payload_encode_snapshot(
    const signal_snapshot_t *snapshot,
    char *output,
    size_t output_size
);

#ifdef __cplusplus
}
#endif

#endif