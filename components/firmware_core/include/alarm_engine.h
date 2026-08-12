#ifndef ALARM_ENGINE_H
#define ALARM_ENGINE_H

#include <stdbool.h>
#include <stdint.h>

#include "app_types.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    ALARM_OPERATOR_GREATER_THAN = 0,
    ALARM_OPERATOR_GREATER_THAN_OR_EQUAL,
    ALARM_OPERATOR_LESS_THAN,
    ALARM_OPERATOR_LESS_THAN_OR_EQUAL,
    ALARM_OPERATOR_EQUAL,
    ALARM_OPERATOR_NOT_EQUAL,
    ALARM_OPERATOR_BOOL_TRUE,
    ALARM_OPERATOR_BOOL_FALSE
} alarm_operator_t;

typedef struct {
    const char *alarm_code;

    signal_id_t source_signal;
    alarm_operator_t comparison_operator;

    float threshold;
    float hysteresis;

    uint32_t activation_delay_ms;
    uint32_t return_delay_ms;

    alarm_severity_t severity;

    bool latched;
    bool enabled;
} alarm_rule_t;

typedef struct {
    uint32_t snapshots_received;
    uint32_t snapshots_evaluated;

    uint32_t rules_evaluated;
    uint32_t alarm_events_generated;
    uint32_t alarm_queue_failures;

    uint32_t active_alarms;
    uint32_t degraded_alarms;
    uint32_t quality_failures;

    bool initialized;
    bool running;
} alarm_engine_status_t;

/**
 * @brief Initializes alarm rules and runtime states.
 */
esp_err_t alarm_engine_init(void);

/**
 * @brief Starts the Alarm Engine FreeRTOS task.
 */
esp_err_t alarm_engine_start(void);

/**
 * @brief Requests stopping the Alarm Engine task.
 */
esp_err_t alarm_engine_stop(void);

/**
 * @brief Evaluates one snapshot synchronously.
 *
 * Useful for tests and future integration testing.
 */
esp_err_t alarm_engine_evaluate(
    const signal_snapshot_t *snapshot
);

/**
 * @brief Returns the number of configured alarm rules.
 */
size_t alarm_engine_rule_count(void);

/**
 * @brief Returns one alarm rule by index.
 */
esp_err_t alarm_engine_get_rule(
    size_t index,
    const alarm_rule_t **output
);

/**
 * @brief Returns current Alarm Engine status.
 */
esp_err_t alarm_engine_get_status(
    alarm_engine_status_t *output
);

/**
 * @brief Returns whether the Alarm Engine task is running.
 */
bool alarm_engine_is_running(void);

#ifdef __cplusplus
}
#endif

#endif