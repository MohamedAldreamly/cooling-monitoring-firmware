#ifndef STORAGE_MANAGER_H
#define STORAGE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "app_types.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t records_received;
    uint32_t records_written;
    uint32_t write_failures;
    uint32_t flush_failures;

    uint32_t recovered_records;
    uint32_t corrupted_records;
    uint32_t incomplete_records;
    uint32_t journal_repairs;

    uint64_t last_written_record_id;
    uint64_t journal_size_bytes;

    bool mounted;
    bool initialized;
    bool running;
    bool degraded;
} storage_manager_status_t;

typedef struct {
    uint32_t valid_records;
    uint32_t corrupted_records;
    uint32_t incomplete_records;

    uint64_t valid_bytes;
    uint64_t original_bytes;
    uint64_t last_valid_record_id;

    bool repair_required;
    bool repair_completed;
} storage_recovery_report_t;

/**
 * @brief Mounts local storage and recovers the journal.
 */
esp_err_t storage_manager_init(void);

/**
 * @brief Starts the single Storage Writer task.
 */
esp_err_t storage_manager_start(void);

/**
 * @brief Requests stopping the Storage Writer task.
 */
esp_err_t storage_manager_stop(void);

/**
 * @brief Appends one record synchronously.
 *
 * Used internally by the writer task and by component tests.
 */
esp_err_t storage_manager_append(
    const durable_record_t *record
);

/**
 * @brief Scans and repairs the journal.
 */
esp_err_t storage_manager_recover(
    storage_recovery_report_t *report
);

/**
 * @brief Returns the current storage status.
 */
esp_err_t storage_manager_get_status(
    storage_manager_status_t *output
);

/**
 * @brief Returns whether the writer task is running.
 */
bool storage_manager_is_running(void);

#ifdef __cplusplus
}
#endif

#endif
