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
 * @brief Result of reading one durable record
 *        from the storage journal.
 */
typedef struct {
    durable_record_t record;

    /*
     * Byte offset where this record starts.
     */
    uint64_t record_offset;

    /*
     * Byte offset immediately after this record.
     * This becomes the next upload cursor after ACK.
     */
    uint64_t next_offset;
} storage_record_read_result_t;

typedef struct {
    uint64_t next_offset;
    bool end_of_journal;
} storage_read_result_t;


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
 * @brief Reads and validates one record from the journal.
 *
 * The function does not delete the record and does not
 * modify the persistent upload cursor.
 *
 * @param offset  Byte offset from which reading starts.
 * @param output  Validated durable record and next offset.
 *
 * @return
 *     - ESP_OK when a valid record is read.
 *     - ESP_ERR_NOT_FOUND when offset is at end of journal.
 *     - ESP_ERR_INVALID_ARG for invalid arguments.
 *     - ESP_ERR_INVALID_STATE when storage is unavailable.
 *     - ESP_ERR_INVALID_SIZE for an invalid stored payload.
 *     - ESP_ERR_INVALID_CRC when CRC validation fails.
 *     - ESP_FAIL for file access or incomplete-record errors.
 */
esp_err_t storage_manager_read_record_at(
    uint64_t offset,
    storage_record_read_result_t *output
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

/**
 * @brief Reads and validates one durable record from the journal.
 *
 * @param offset Byte offset of the record header inside journal.bin.
 * @param output Reconstructed durable record.
 * @param read_result Offset of the following record and EOF state.
 */
esp_err_t storage_manager_read_record(
    uint64_t offset,
    durable_record_t *output,
    storage_read_result_t *read_result
);

#ifdef __cplusplus
}
#endif

#endif
