#ifndef TELEMETRY_PUBLISHER_H
#define TELEMETRY_PUBLISHER_H

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    /*
     * Number of periodic Telemetry cycles.
     */
    uint32_t cycles;

    /*
     * Successfully generated snapshots.
     */
    uint32_t snapshots_built;

    /*
     * Snapshots successfully converted to JSON.
     */
    uint32_t payloads_encoded;

    /*
     * MQTT publish attempts.
     */
    uint32_t publish_requests;

    /*
     * Payloads successfully queued to MQTT.
     */
    uint32_t publishes_queued;

    /*
     * Cycles skipped because MQTT was not ready.
     */
    uint32_t offline_skips;

    /*
     * Error counters.
     */
    uint32_t snapshot_failures;
    uint32_t encoding_failures;
    uint32_t publish_failures;

    /*
     * Information about the latest publication.
     */
    uint32_t last_snapshot_sequence;
    int last_mqtt_message_id;

    bool initialized;
    bool running;
} telemetry_publisher_status_t;

/**
 * @brief Initializes Telemetry Publisher state.
 */
esp_err_t telemetry_publisher_init(void);

/**
 * @brief Creates and starts the periodic Telemetry task.
 */
esp_err_t telemetry_publisher_start(void);

/**
 * @brief Requests stopping the Telemetry task.
 */
esp_err_t telemetry_publisher_stop(void);

/**
 * @brief Copies the current Telemetry Publisher status.
 */
esp_err_t telemetry_publisher_get_status(
    telemetry_publisher_status_t *output
);

/**
 * @brief Returns whether the Telemetry task is running.
 */
bool telemetry_publisher_is_running(void);

#ifdef __cplusplus
}
#endif

#endif