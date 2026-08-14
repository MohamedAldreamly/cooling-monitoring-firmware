#ifndef CLOUD_UPLOADER_H
#define CLOUD_UPLOADER_H

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t records_read;
    uint32_t payloads_encoded;

    uint32_t empty_journal_checks;
    uint32_t read_failures;
    uint32_t unsupported_records;
    uint32_t encoding_failures;

    uint64_t current_record_id;
    uint64_t current_record_offset;
    uint64_t current_next_offset;

    bool initialized;
    bool running;
    bool record_waiting_for_ack;
} cloud_uploader_status_t;

/**
 * @brief Initializes the cloud uploader.
 *
 * Storage Manager and Upload Cursor must already
 * be initialized.
 */
esp_err_t cloud_uploader_init(void);

/**
 * @brief Starts the Cloud Uploader task.
 *
 * During the preview stage, the task reads the oldest
 * unacknowledged alarm record and converts it to JSON.
 * It does not publish to MQTT and does not advance
 * the persistent upload cursor.
 */
esp_err_t cloud_uploader_start(void);

/**
 * @brief Requests stopping the Cloud Uploader task.
 */
esp_err_t cloud_uploader_stop(void);

/**
 * @brief Returns the current uploader status.
 */
esp_err_t cloud_uploader_get_status(
    cloud_uploader_status_t *output
);

/**
 * @brief Returns whether the uploader task is running.
 */
bool cloud_uploader_is_running(void);

#ifdef __cplusplus
}
#endif

#endif