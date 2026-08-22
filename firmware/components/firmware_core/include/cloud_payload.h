#ifndef CLOUD_PAYLOAD_H
#define CLOUD_PAYLOAD_H

#include <stddef.h>

#include "app_types.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Converts a stored alarm durable record
 *        into the JSON payload sent to AWS IoT.
 *
 * @param record       Alarm record read from local storage.
 * @param buffer       Destination JSON buffer.
 * @param buffer_size  Total destination buffer size.
 *
 * @return
 *     - ESP_OK on success.
 *     - ESP_ERR_INVALID_ARG for invalid arguments or record type.
 *     - ESP_ERR_INVALID_SIZE for an invalid payload size.
 *     - ESP_ERR_INVALID_VERSION for an unsupported payload version.
 *     - ESP_ERR_NOT_FOUND for an invalid source signal.
 *     - ESP_ERR_NO_MEM if JSON creation fails.
 */
esp_err_t cloud_payload_encode_alarm(
    const durable_record_t *record,
    char *buffer,
    size_t buffer_size
);

#ifdef __cplusplus
}
#endif

#endif