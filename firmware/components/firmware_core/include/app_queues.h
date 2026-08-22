#ifndef APP_QUEUES_H
#define APP_QUEUES_H

#include <stdbool.h>
#include <stdint.h>

#include "../../components/firmware_core/include/app_types.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Queue capacities are initial simulation baselines.
 * They will later be tuned using runtime queue-depth metrics.
 */
#define APP_RAW_SAMPLE_QUEUE_LENGTH        64U
#define APP_SIGNAL_VALUE_QUEUE_LENGTH      64U
#define APP_SIGNAL_SNAPSHOT_QUEUE_LENGTH   8U
#define APP_ALARM_EVENT_QUEUE_LENGTH       16U
#define APP_RECORD_QUEUE_LENGTH            32U

typedef struct {
    uint32_t raw_sample_sent;
    uint32_t raw_sample_received;
    uint32_t raw_sample_dropped;

    uint32_t signal_value_sent;
    uint32_t signal_value_received;
    uint32_t signal_value_dropped;

    uint32_t alarm_event_sent;
    uint32_t alarm_event_received;
    uint32_t alarm_event_dropped;

    uint32_t record_sent;
    uint32_t record_received;
    uint32_t record_dropped;
    
    uint32_t signal_snapshot_sent;
	uint32_t signal_snapshot_received;
	uint32_t signal_snapshot_dropped;

} app_queue_counters_t;

/**
 * @brief Creates all application queues.
 *
 * Must be called once during startup before creating producer/consumer tasks.
 */
esp_err_t app_queues_init(void);

/**
 * @brief Returns whether the application queues were initialized.
 */
bool app_queues_is_initialized(void);

/**
 * @brief Sends a raw sample to the acquisition queue.
 */
esp_err_t app_queues_send_raw_sample(
    const raw_sample_t *sample,
    TickType_t timeout_ticks
);

/**
 * @brief Receives a raw sample from the acquisition queue.
 */
esp_err_t app_queues_receive_raw_sample(
    raw_sample_t *sample,
    TickType_t timeout_ticks
);

/**
 * @brief Sends a processed signal value.
 */
esp_err_t app_queues_send_signal_value(
    const signal_value_t *value,
    TickType_t timeout_ticks
);

/**
 * @brief Receives a processed signal value.
 */
esp_err_t app_queues_receive_signal_value(
    signal_value_t *value,
    TickType_t timeout_ticks
);

/**
 * @brief Sends an alarm event.
 */
esp_err_t app_queues_send_alarm_event(
    const alarm_event_t *event,
    TickType_t timeout_ticks
);

/**
 * @brief Receives an alarm event.
 */
esp_err_t app_queues_receive_alarm_event(
    alarm_event_t *event,
    TickType_t timeout_ticks
);

/**
 * @brief Sends a durable record.
 */
esp_err_t app_queues_send_record(
    const durable_record_t *record,
    TickType_t timeout_ticks
);

/**
 * @brief Receives a durable record.
 */
esp_err_t app_queues_receive_record(
    durable_record_t *record,
    TickType_t timeout_ticks
);

/**
 * @brief Sends a signal snapshot.
 */
esp_err_t app_queues_send_signal_snapshot(
    const signal_snapshot_t *snapshot,
    TickType_t timeout_ticks
);

/**
 * @brief Receives a signal snapshot.
 */
esp_err_t app_queues_receive_signal_snapshot(
    signal_snapshot_t *snapshot,
    TickType_t timeout_ticks
);

/**
 * @brief Returns the number of queued snapshots.
 */
UBaseType_t app_queues_signal_snapshot_depth(void);

/**
 * @brief Returns the current number of raw samples waiting.
 */
UBaseType_t app_queues_raw_sample_depth(void);

/**
 * @brief Returns queue counters.
 */
esp_err_t app_queues_get_counters(app_queue_counters_t *out);

#ifdef __cplusplus
}
#endif

#endif