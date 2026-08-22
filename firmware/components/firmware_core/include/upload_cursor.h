#ifndef UPLOAD_CURSOR_H
#define UPLOAD_CURSOR_H

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Persistent position of the cloud uploader.
 *
 * next_offset points to the first journal record that
 * has not yet received a valid Application ACK.
 */
typedef struct {
    uint64_t next_offset;
    uint64_t last_acked_record_id;

    bool initialized;
} upload_cursor_status_t;

/**
 * @brief Initializes and restores the upload cursor from NVS.
 *
 * If no cursor exists, the cursor starts at offset zero.
 */
esp_err_t upload_cursor_init(void);

/**
 * @brief Returns the current cursor state.
 */
esp_err_t upload_cursor_get(
    upload_cursor_status_t *output
);

/**
 * @brief Atomically persists an acknowledged record position.
 *
 * This function must only be called after receiving and
 * validating the Application ACK for the corresponding record.
 */
esp_err_t upload_cursor_commit(
    uint64_t acknowledged_record_id,
    uint64_t next_offset
);

/**
 * @brief Resets and persists the upload cursor.
 *
 * Used when the saved cursor is incompatible with the
 * current journal, for example after storage recovery
 * or reformatting.
 */
esp_err_t upload_cursor_reset(void);

/**
 * @brief Returns whether the cursor module is initialized.
 */
bool upload_cursor_is_initialized(void);

#ifdef __cplusplus
}
#endif

#endif