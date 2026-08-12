#ifndef RECORD_BUILDER_H
#define RECORD_BUILDER_H

#include <stdbool.h>
#include <stdint.h>

#include "app_types.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t alarm_events_received;
    uint32_t records_created;
    uint32_t records_published;

    uint32_t invalid_events;
    uint32_t payload_failures;
    uint32_t record_queue_failures;
    uint32_t identity_failures;

    uint64_t current_record_id;
    uint32_t current_boot_id;
    uint32_t current_boot_sequence;

    bool initialized;
    bool running;
} record_builder_status_t;

/**
 * @brief Initializes Record Builder.
 *
 * boot_id and record_id are supplied by System Identity Manager.
 */
esp_err_t record_builder_init(void);

/**
 * @brief Starts the Record Builder FreeRTOS task.
 */
esp_err_t record_builder_start(void);

/**
 * @brief Requests stopping the Record Builder task.
 */
esp_err_t record_builder_stop(void);

/**
 * @brief Converts one alarm event into a durable record.
 *
 * This synchronous API is useful for unit testing.
 */
esp_err_t record_builder_build_alarm_record(
    const alarm_event_t *event,
    durable_record_t *output
);

/**
 * @brief Returns current Record Builder status.
 */
esp_err_t record_builder_get_status(
    record_builder_status_t *output
);

/**
 * @brief Returns whether the task is running.
 */
bool record_builder_is_running(void);

#ifdef __cplusplus
}
#endif

#endif