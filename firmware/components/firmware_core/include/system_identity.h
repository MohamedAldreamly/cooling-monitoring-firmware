#ifndef SYSTEM_IDENTITY_H
#define SYSTEM_IDENTITY_H

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t boot_id;

    uint64_t last_issued_record_id;
    uint64_t reserved_record_id_limit;

    uint32_t reserved_blocks;
    uint32_t nvs_write_failures;

    bool initialized;
    bool degraded;
} system_identity_status_t;

/**
 * @brief Initializes NVS and persistent device counters.
 *
 * Each successful initialization represents a new firmware boot,
 * therefore boot_id is incremented and committed to NVS.
 */
esp_err_t system_identity_init(void);

/**
 * @brief Returns the current boot identifier.
 */
esp_err_t system_identity_get_boot_id(
    uint32_t *boot_id
);

/**
 * @brief Issues a globally increasing record identifier.
 *
 * Record IDs are reserved in blocks in NVS to avoid writing
 * to flash for every generated record.
 */
esp_err_t system_identity_next_record_id(
    uint64_t *record_id
);

/**
 * @brief Returns current identity-manager status.
 */
esp_err_t system_identity_get_status(
    system_identity_status_t *output
);

/**
 * @brief Returns whether the manager was initialized.
 */
bool system_identity_is_initialized(void);

#ifdef __cplusplus
}
#endif

#endif