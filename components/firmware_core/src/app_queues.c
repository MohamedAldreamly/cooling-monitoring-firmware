#include "app_queues.h"

#include <string.h>

#include "esp_log.h"

static const char *TAG = "APP_QUEUES";

static QueueHandle_t s_raw_sample_queue = NULL;
static QueueHandle_t s_signal_value_queue = NULL;
static QueueHandle_t s_signal_snapshot_queue = NULL;
static QueueHandle_t s_alarm_event_queue = NULL;
static QueueHandle_t s_record_queue = NULL;

static bool s_initialized = false;
static app_queue_counters_t s_counters;

static void app_queues_delete_all(void)
{
    if (s_raw_sample_queue != NULL) {
        vQueueDelete(s_raw_sample_queue);
        s_raw_sample_queue = NULL;
    }

    if (s_signal_value_queue != NULL) {
        vQueueDelete(s_signal_value_queue);
        s_signal_value_queue = NULL;
    }

    if (s_alarm_event_queue != NULL) {
        vQueueDelete(s_alarm_event_queue);
        s_alarm_event_queue = NULL;
    }

    if (s_record_queue != NULL) {
        vQueueDelete(s_record_queue);
        s_record_queue = NULL;
    }
    
    if (s_signal_snapshot_queue != NULL) {
    vQueueDelete(s_signal_snapshot_queue);
    s_signal_snapshot_queue = NULL;
}

    s_initialized = false;
}

esp_err_t app_queues_init(void)
{
    if (s_initialized) {
        return ESP_OK;
    }

    memset(&s_counters, 0, sizeof(s_counters));

    s_raw_sample_queue = xQueueCreate(
        APP_RAW_SAMPLE_QUEUE_LENGTH,
        sizeof(raw_sample_t)
    );

    s_signal_value_queue = xQueueCreate(
        APP_SIGNAL_VALUE_QUEUE_LENGTH,
        sizeof(signal_value_t)
    );

    s_alarm_event_queue = xQueueCreate(
        APP_ALARM_EVENT_QUEUE_LENGTH,
        sizeof(alarm_event_t)
    );

    s_record_queue = xQueueCreate(
        APP_RECORD_QUEUE_LENGTH,
        sizeof(durable_record_t)
    );
    
    s_signal_snapshot_queue = xQueueCreate(
    APP_SIGNAL_SNAPSHOT_QUEUE_LENGTH,
    sizeof(signal_snapshot_t)
	);

	if ((s_raw_sample_queue == NULL) ||
    	(s_signal_value_queue == NULL) ||
    	(s_signal_snapshot_queue == NULL) ||
    	(s_alarm_event_queue == NULL) ||
    	(s_record_queue == NULL)) {

    	ESP_LOGE(
        	TAG,
        	"Failed to create one or more application queues"
    		);

    	app_queues_delete_all();

    	return ESP_ERR_NO_MEM;

    }

    s_initialized = true;

    ESP_LOGI(
    TAG,
    "Queues initialized: raw=%u, value=%u, snapshot=%u, alarm=%u, record=%u",
    APP_RAW_SAMPLE_QUEUE_LENGTH,
    APP_SIGNAL_VALUE_QUEUE_LENGTH,
    APP_SIGNAL_SNAPSHOT_QUEUE_LENGTH,
    APP_ALARM_EVENT_QUEUE_LENGTH,
    APP_RECORD_QUEUE_LENGTH
	);

    return ESP_OK;
}

bool app_queues_is_initialized(void)
{
    return s_initialized;
}

esp_err_t app_queues_send_raw_sample(
    const raw_sample_t *sample,
    TickType_t timeout_ticks
)
{
    if (sample == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!s_initialized || s_raw_sample_queue == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    if (xQueueSend(
            s_raw_sample_queue,
            sample,
            timeout_ticks
        ) != pdPASS) {

        s_counters.raw_sample_dropped++;
        return ESP_ERR_TIMEOUT;
    }

    s_counters.raw_sample_sent++;
    return ESP_OK;
}

esp_err_t app_queues_receive_raw_sample(
    raw_sample_t *sample,
    TickType_t timeout_ticks
)
{
    if (sample == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!s_initialized || s_raw_sample_queue == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    if (xQueueReceive(
            s_raw_sample_queue,
            sample,
            timeout_ticks
        ) != pdPASS) {

        return ESP_ERR_TIMEOUT;
    }

    s_counters.raw_sample_received++;
    return ESP_OK;
}

esp_err_t app_queues_send_signal_value(
    const signal_value_t *value,
    TickType_t timeout_ticks
)
{
    if (value == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!s_initialized || s_signal_value_queue == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    if (xQueueSend(
            s_signal_value_queue,
            value,
            timeout_ticks
        ) != pdPASS) {

        s_counters.signal_value_dropped++;
        return ESP_ERR_TIMEOUT;
    }

    s_counters.signal_value_sent++;
    return ESP_OK;
}

esp_err_t app_queues_receive_signal_value(
    signal_value_t *value,
    TickType_t timeout_ticks
)
{
    if (value == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!s_initialized || s_signal_value_queue == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    if (xQueueReceive(
            s_signal_value_queue,
            value,
            timeout_ticks
        ) != pdPASS) {

        return ESP_ERR_TIMEOUT;
    }

    s_counters.signal_value_received++;
    return ESP_OK;
}

esp_err_t app_queues_send_alarm_event(
    const alarm_event_t *event,
    TickType_t timeout_ticks
)
{
    if (event == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!s_initialized || s_alarm_event_queue == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    if (xQueueSend(
            s_alarm_event_queue,
            event,
            timeout_ticks
        ) != pdPASS) {

        s_counters.alarm_event_dropped++;
        return ESP_ERR_TIMEOUT;
    }

    s_counters.alarm_event_sent++;
    return ESP_OK;
}

esp_err_t app_queues_receive_alarm_event(
    alarm_event_t *event,
    TickType_t timeout_ticks
)
{
    if (event == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!s_initialized || s_alarm_event_queue == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    if (xQueueReceive(
            s_alarm_event_queue,
            event,
            timeout_ticks
        ) != pdPASS) {

        return ESP_ERR_TIMEOUT;
    }

    s_counters.alarm_event_received++;
    return ESP_OK;
}

esp_err_t app_queues_send_record(
    const durable_record_t *record,
    TickType_t timeout_ticks
)
{
    if (record == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!s_initialized || s_record_queue == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    if (xQueueSend(
            s_record_queue,
            record,
            timeout_ticks
        ) != pdPASS) {

        s_counters.record_dropped++;
        return ESP_ERR_TIMEOUT;
    }

    s_counters.record_sent++;
    return ESP_OK;
}

esp_err_t app_queues_receive_record(
    durable_record_t *record,
    TickType_t timeout_ticks
)
{
    if (record == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!s_initialized || s_record_queue == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    if (xQueueReceive(
            s_record_queue,
            record,
            timeout_ticks
        ) != pdPASS) {

        return ESP_ERR_TIMEOUT;
    }

    s_counters.record_received++;
    return ESP_OK;
}

UBaseType_t app_queues_raw_sample_depth(void)
{
    if (!s_initialized || s_raw_sample_queue == NULL) {
        return 0;
    }

    return uxQueueMessagesWaiting(s_raw_sample_queue);
}

esp_err_t app_queues_get_counters(app_queue_counters_t *out)
{
    if (out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    *out = s_counters;
    return ESP_OK;
}

esp_err_t app_queues_send_signal_snapshot(
    const signal_snapshot_t *snapshot,
    TickType_t timeout_ticks
)
{
    if (snapshot == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!s_initialized ||
        s_signal_snapshot_queue == NULL) {

        return ESP_ERR_INVALID_STATE;
    }

    if (xQueueSend(
            s_signal_snapshot_queue,
            snapshot,
            timeout_ticks
        ) != pdPASS) {

        s_counters.signal_snapshot_dropped++;

        return ESP_ERR_TIMEOUT;
    }

    s_counters.signal_snapshot_sent++;

    return ESP_OK;
}

esp_err_t app_queues_receive_signal_snapshot(
    signal_snapshot_t *snapshot,
    TickType_t timeout_ticks
)
{
    if (snapshot == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!s_initialized ||
        s_signal_snapshot_queue == NULL) {

        return ESP_ERR_INVALID_STATE;
    }

    if (xQueueReceive(
            s_signal_snapshot_queue,
            snapshot,
            timeout_ticks
        ) != pdPASS) {

        return ESP_ERR_TIMEOUT;
    }

    s_counters.signal_snapshot_received++;

    return ESP_OK;
}

UBaseType_t app_queues_signal_snapshot_depth(void)
{
    if (!s_initialized ||
        s_signal_snapshot_queue == NULL) {

        return 0;
    }

    return uxQueueMessagesWaiting(
        s_signal_snapshot_queue
    );
}