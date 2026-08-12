#ifndef SIGNAL_PROCESSING_H
#define SIGNAL_PROCESSING_H

#include <stdbool.h>
#include <stdint.h>

#include "app_types.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t samples_received;
    uint32_t samples_processed;
    uint32_t samples_rejected;

    uint32_t communication_faults;
    uint32_t sensor_faults;
    uint32_t out_of_range_values;
    uint32_t queue_failures;

    bool initialized;
    bool running;
} signal_processing_status_t;

/**
 * @brief Initializes processing state and filter buffers.
 */
esp_err_t signal_processing_init(void);

/**
 * @brief Starts the Signal Processing FreeRTOS task.
 */
esp_err_t signal_processing_start(void);

/**
 * @brief Requests stopping the Signal Processing task.
 */
esp_err_t signal_processing_stop(void);

/**
 * @brief Processes one raw sample synchronously.
 *
 * Useful for unit tests and future component tests.
 */
esp_err_t signal_processing_process(
    const raw_sample_t *input,
    signal_value_t *output
);

/**
 * @brief Returns the current processing status.
 */
esp_err_t signal_processing_get_status(
    signal_processing_status_t *output
);

/**
 * @brief Returns whether the processing task is running.
 */
bool signal_processing_is_running(void);

#ifdef __cplusplus
}
#endif

#endif