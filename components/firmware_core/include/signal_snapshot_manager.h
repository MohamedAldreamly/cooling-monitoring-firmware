#ifndef SIGNAL_SNAPSHOT_MANAGER_H
#define SIGNAL_SNAPSHOT_MANAGER_H

#include <stdbool.h>
#include <stdint.h>

#include "app_types.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t values_received;
    uint32_t snapshots_created;
    uint32_t snapshots_published;
    uint32_t snapshot_queue_failures;

    uint32_t stale_transitions;
    uint32_t invalid_signal_ids;

    uint32_t current_snapshot_sequence;

    bool initialized;
    bool running;
} signal_snapshot_manager_status_t;

/**
 * @brief Initializes the latest-value table.
 */
	esp_err_t signal_snapshot_manager_init(void);

/**
 * @brief Starts the Snapshot Manager task.
 */
esp_err_t signal_snapshot_manager_start(void);

/**
 * @brief Requests stopping the task.
 */
esp_err_t signal_snapshot_manager_stop(void);

/**
 * @brief Returns the latest value for one signal.
 */
esp_err_t signal_snapshot_manager_get_latest(
    signal_id_t signal_id,
    signal_value_t *output
);

/**
 * @brief Builds a snapshot synchronously.
 */
esp_err_t signal_snapshot_manager_build(
    uint64_t now_uptime_ms,
    signal_snapshot_t *output
);

/**
 * @brief Returns manager status.
 */
esp_err_t signal_snapshot_manager_get_status(
    signal_snapshot_manager_status_t *output
);

bool signal_snapshot_manager_is_running(void);

#ifdef __cplusplus
}
#endif

#endif
