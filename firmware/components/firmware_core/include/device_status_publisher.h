#ifndef DEVICE_STATUS_PUBLISHER_H
#define DEVICE_STATUS_PUBLISHER_H

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    /* Number of periodic Device Status cycles. */
    uint32_t cycles;

    /* Status payloads successfully encoded as JSON. */
    uint32_t payloads_encoded;

    /* MQTT publish requests attempted by this component. */
    uint32_t publish_requests;

    /* Status payloads successfully queued to MQTT. */
    uint32_t publishes_queued;

    /* Cycles skipped because the cloud transport was not ready. */
    uint32_t offline_skips;

    /* Error counters. */
    uint32_t encoding_failures;
    uint32_t publish_failures;

    /* Information about the latest successful publish request. */
    int last_mqtt_message_id;

    bool initialized;
    bool running;
} device_status_publisher_status_t;

/**
 * @brief Initializes Device Status Publisher state.
 */
esp_err_t device_status_publisher_init(void);

/**
 * @brief Creates and starts the periodic Device Status task.
 */
esp_err_t device_status_publisher_start(void);

/**
 * @brief Requests stopping the Device Status task.
 */
esp_err_t device_status_publisher_stop(void);

/**
 * @brief Copies the current Device Status Publisher state.
 */
esp_err_t device_status_publisher_get_status(
    device_status_publisher_status_t *output
);

/**
 * @brief Returns whether the Device Status task is running.
 */
bool device_status_publisher_is_running(void);

#ifdef __cplusplus
}
#endif

#endif