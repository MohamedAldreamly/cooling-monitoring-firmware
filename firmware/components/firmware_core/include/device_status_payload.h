#ifndef DEVICE_STATUS_PAYLOAD_H
#define DEVICE_STATUS_PAYLOAD_H

#include <stddef.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Builds the latest device-health state as a JSON payload.
 *
 * The generated payload describes the current live state of the device,
 * including connectivity, local journal usage, and upload-cursor progress.
 * It is not stored in journal.bin and it does not modify the upload cursor.
 *
 * @param output Buffer where the generated JSON will be written.
 * @param output_size Total size of the output buffer.
 *
 * @return ESP_OK when the JSON payload is generated successfully.
 * @return ESP_ERR_INVALID_ARG when an argument is invalid.
 * @return ESP_ERR_INVALID_STATE when a required subsystem is not ready.
 * @return ESP_ERR_NO_MEM when JSON objects cannot be allocated.
 * @return ESP_ERR_INVALID_SIZE when the output buffer is too small.
 */
esp_err_t device_status_payload_encode(
    char *output,
    size_t output_size
);

#ifdef __cplusplus
}
#endif

#endif