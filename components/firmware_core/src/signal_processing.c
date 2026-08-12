#include "signal_processing.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

#include "app_queues.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "signal_registry.h"

static const char *TAG = "SIGNAL_PROCESSING";

#define SIGNAL_PROCESSING_TASK_NAME             "signal_processing"
#define SIGNAL_PROCESSING_TASK_STACK_SIZE       4096U
#define SIGNAL_PROCESSING_TASK_PRIORITY         6U

#define SIGNAL_PROCESSING_RECEIVE_TIMEOUT_MS    1000U
#define SIGNAL_PROCESSING_SEND_TIMEOUT_MS       20U

/*
 * أكبر Filter Window مستخدم حاليًا في Signal Registry هو 5.
 * نترك مساحة حتى 9 عينات للتطوير اللاحق.
 */
#define SIGNAL_FILTER_MAX_WINDOW_SIZE           9U

typedef struct {
    float samples[SIGNAL_FILTER_MAX_WINDOW_SIZE];

    uint8_t sample_count;
    uint8_t write_index;
    bool initialized;
} signal_filter_state_t;

static TaskHandle_t s_task_handle = NULL;

static signal_processing_status_t s_status;

static signal_filter_state_t
    s_filter_states[SIGNAL_ID_COUNT];

static bool s_stop_requested = false;

static bool signal_processing_valid_id(signal_id_t signal_id)
{
    return signal_id < SIGNAL_ID_COUNT;
}

static void signal_processing_clear_output(
    const raw_sample_t *input,
    signal_value_t *output
)
{
    memset(output, 0, sizeof(*output));

    output->signal_id = input->signal_id;
    output->uptime_ms = input->uptime_ms;
    output->observed_at_ms = input->observed_at_ms;

    output->quality = SIGNAL_QUALITY_INITIALIZING;
    output->has_value = false;
}

static esp_err_t signal_processing_validate_type(
    const raw_sample_t *input,
    const signal_definition_t *definition
)
{
    if (input->raw_data_type != definition->data_type) {
        ESP_LOGW(
            TAG,
            "Data type mismatch: signal=%s raw_type=%d expected=%d",
            definition->name,
            (int)input->raw_data_type,
            (int)definition->data_type
        );

        return ESP_ERR_INVALID_ARG;
    }

    return ESP_OK;
}

static float signal_processing_apply_moving_average(
    signal_id_t signal_id,
    float input_value,
    uint8_t requested_window_size
)
{
    if (!signal_processing_valid_id(signal_id)) {
        return input_value;
    }

    uint8_t window_size = requested_window_size;

    if (window_size == 0U) {
        window_size = 1U;
    }

    if (window_size > SIGNAL_FILTER_MAX_WINDOW_SIZE) {
        window_size = SIGNAL_FILTER_MAX_WINDOW_SIZE;
    }

    signal_filter_state_t *state =
        &s_filter_states[signal_id];

    state->samples[state->write_index] = input_value;

    state->write_index++;

    if (state->write_index >= window_size) {
        state->write_index = 0U;
    }

    if (state->sample_count < window_size) {
        state->sample_count++;
    }

    state->initialized = true;

    float sum = 0.0f;

    for (uint8_t index = 0U;
         index < state->sample_count;
         index++) {

        sum += state->samples[index];
    }

    if (state->sample_count == 0U) {
        return input_value;
    }

    return sum / (float)state->sample_count;
}

static float signal_processing_apply_filter(
    signal_id_t signal_id,
    float input_value,
    const signal_definition_t *definition
)
{
    switch (definition->filter_type) {
        case SIGNAL_FILTER_NONE:
            return input_value;

        case SIGNAL_FILTER_MOVING_AVERAGE:
            return signal_processing_apply_moving_average(
                signal_id,
                input_value,
                definition->filter_window_size
            );

        case SIGNAL_FILTER_MEDIAN:
            /*
             * Median Filter سيُنفذ عندما نحتاجه فعليًا.
             * لا توجد إشارة حالية تستخدمه في الـRegistry.
             */
            return input_value;

        default:
            return input_value;
    }
}

static esp_err_t signal_processing_process_float(
    const raw_sample_t *input,
    const signal_definition_t *definition,
    signal_value_t *output
)
{
    const float raw_value =
        input->raw_value.float32;

    if (!isfinite(raw_value)) {
        output->quality = SIGNAL_QUALITY_BAD;
        output->has_value = false;

        return ESP_ERR_INVALID_ARG;
    }

    /*
     * المعادلة المعتمدة:
     *
     * engineering_value = raw_value × gain + offset
     */
    const float scaled_value =
        (raw_value * definition->gain) +
        definition->offset;

    if (!isfinite(scaled_value)) {
        output->quality = SIGNAL_QUALITY_BAD;
        output->has_value = false;

        return ESP_ERR_INVALID_ARG;
    }

    const float filtered_value =
        signal_processing_apply_filter(
            input->signal_id,
            scaled_value,
            definition
        );

    output->data_type = SIGNAL_DATA_TYPE_FLOAT32;
    output->value.float32 = filtered_value;
    output->unit = definition->unit;
    output->has_value = true;

    if ((filtered_value < definition->valid_min) ||
        (filtered_value > definition->valid_max)) {

        output->quality =
            SIGNAL_QUALITY_OUT_OF_RANGE;

        s_status.out_of_range_values++;

        return ESP_OK;
    }

    output->quality = SIGNAL_QUALITY_GOOD;

    return ESP_OK;
}

static esp_err_t signal_processing_process_bool(
    const raw_sample_t *input,
    const signal_definition_t *definition,
    signal_value_t *output
)
{
    output->data_type = SIGNAL_DATA_TYPE_BOOL;
    output->value.boolean = input->raw_value.boolean;
    output->unit = definition->unit;
    output->quality = SIGNAL_QUALITY_GOOD;
    output->has_value = true;

    return ESP_OK;
}

static esp_err_t signal_processing_process_int32(
    const raw_sample_t *input,
    const signal_definition_t *definition,
    signal_value_t *output
)
{
    output->data_type = SIGNAL_DATA_TYPE_INT32;
    output->value.int32 = input->raw_value.int32;
    output->unit = definition->unit;
    output->quality = SIGNAL_QUALITY_GOOD;
    output->has_value = true;

    const float numeric_value =
        (float)input->raw_value.int32;

    if ((numeric_value < definition->valid_min) ||
        (numeric_value > definition->valid_max)) {

        output->quality =
            SIGNAL_QUALITY_OUT_OF_RANGE;

        s_status.out_of_range_values++;
    }

    return ESP_OK;
}

static esp_err_t signal_processing_process_uint32(
    const raw_sample_t *input,
    const signal_definition_t *definition,
    signal_value_t *output
)
{
    output->data_type = SIGNAL_DATA_TYPE_UINT32;
    output->value.uint32 = input->raw_value.uint32;
    output->unit = definition->unit;
    output->quality = SIGNAL_QUALITY_GOOD;
    output->has_value = true;

    const float numeric_value =
        (float)input->raw_value.uint32;

    if ((numeric_value < definition->valid_min) ||
        (numeric_value > definition->valid_max)) {

        output->quality =
            SIGNAL_QUALITY_OUT_OF_RANGE;

        s_status.out_of_range_values++;
    }

    return ESP_OK;
}

esp_err_t signal_processing_process(
    const raw_sample_t *input,
    signal_value_t *output
)
{
    if ((input == NULL) || (output == NULL)) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!s_status.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    signal_processing_clear_output(input, output);

    if (!signal_processing_valid_id(input->signal_id)) {
        output->quality = SIGNAL_QUALITY_BAD;

        return ESP_ERR_NOT_FOUND;
    }

    const signal_definition_t *definition = NULL;

    esp_err_t registry_result =
        signal_registry_get(
            input->signal_id,
            &definition
        );

    if ((registry_result != ESP_OK) ||
        (definition == NULL)) {

        output->quality = SIGNAL_QUALITY_BAD;

        return ESP_ERR_NOT_FOUND;
    }

    output->data_type = definition->data_type;
    output->unit = definition->unit;

    /*
     * فشل الاتصال يأخذ الأولوية على القيمة نفسها.
     * لا نمرر قيمة قديمة أو وهمية كأنها قراءة صحيحة.
     */
    if (!input->communication_ok ||
        !input->crc_ok) {

        output->quality =
            SIGNAL_QUALITY_COMMUNICATION_FAULT;

        output->has_value = false;

        s_status.communication_faults++;

        return ESP_OK;
    }

    /*
     * source_fault يعني أن الاتصال قد يكون سليمًا،
     * لكن الحساس أو دائرة الإدخال أبلغت عن عطل.
     */
    if (input->source_fault) {
        output->quality =
            SIGNAL_QUALITY_SENSOR_FAULT;

        output->has_value = false;

        s_status.sensor_faults++;

        return ESP_OK;
    }

    esp_err_t type_result =
        signal_processing_validate_type(
            input,
            definition
        );

    if (type_result != ESP_OK) {
        output->quality = SIGNAL_QUALITY_BAD;
        output->has_value = false;

        return type_result;
    }

    switch (definition->data_type) {
        case SIGNAL_DATA_TYPE_FLOAT32:
            return signal_processing_process_float(
                input,
                definition,
                output
            );

        case SIGNAL_DATA_TYPE_BOOL:
            return signal_processing_process_bool(
                input,
                definition,
                output
            );

        case SIGNAL_DATA_TYPE_INT32:
            return signal_processing_process_int32(
                input,
                definition,
                output
            );

        case SIGNAL_DATA_TYPE_UINT32:
            return signal_processing_process_uint32(
                input,
                definition,
                output
            );

        default:
            output->quality = SIGNAL_QUALITY_BAD;
            output->has_value = false;

            return ESP_ERR_NOT_SUPPORTED;
    }
}

static void signal_processing_task(void *argument)
{
    (void)argument;

    raw_sample_t raw_sample;
    signal_value_t signal_value;

    s_status.running = true;
    s_stop_requested = false;

    ESP_LOGI(TAG, "Signal Processing task started");

    while (!s_stop_requested) {
        esp_err_t receive_result =
            app_queues_receive_raw_sample(
                &raw_sample,
                pdMS_TO_TICKS(
                    SIGNAL_PROCESSING_RECEIVE_TIMEOUT_MS
                )
            );

        if (receive_result == ESP_ERR_TIMEOUT) {
            /*
             * Timeout طبيعي، ويسمح للـTask بفحص stop request.
             */
            continue;
        }

        if (receive_result != ESP_OK) {
            ESP_LOGW(
                TAG,
                "Failed to receive raw sample: %s",
                esp_err_to_name(receive_result)
            );

            continue;
        }

        s_status.samples_received++;

        esp_err_t processing_result =
            signal_processing_process(
                &raw_sample,
                &signal_value
            );

        if (processing_result != ESP_OK) {
            s_status.samples_rejected++;

            ESP_LOGW(
                TAG,
                "Sample processing failed: signal=%s error=%s",
                signal_registry_name(
                    raw_sample.signal_id
                ),
                esp_err_to_name(processing_result)
            );

            /*
             * حتى عند خطأ المعالجة نرسل Signal Value
             * إذا كانت Quality محددة، لكي لا تختفي الإشارة.
             */
        }

        esp_err_t queue_result =
            app_queues_send_signal_value(
                &signal_value,
                pdMS_TO_TICKS(
                    SIGNAL_PROCESSING_SEND_TIMEOUT_MS
                )
            );

        if (queue_result != ESP_OK) {
            s_status.queue_failures++;

            ESP_LOGW(
                TAG,
                "Signal value queue full: signal=%s",
                signal_registry_name(
                    signal_value.signal_id
                )
            );

            continue;
        }

        s_status.samples_processed++;
    }

    s_status.running = false;
    s_task_handle = NULL;

    ESP_LOGI(TAG, "Signal Processing task stopped");

    vTaskDelete(NULL);
}

esp_err_t signal_processing_init(void)
{
    if (s_status.running) {
        return ESP_ERR_INVALID_STATE;
    }

    memset(&s_status, 0, sizeof(s_status));
    memset(
        s_filter_states,
        0,
        sizeof(s_filter_states)
    );

    s_status.initialized = true;
    s_stop_requested = false;

    ESP_LOGI(TAG, "Signal Processing initialized");

    return ESP_OK;
}

esp_err_t signal_processing_start(void)
{
    if (!s_status.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (!app_queues_is_initialized()) {
        return ESP_ERR_INVALID_STATE;
    }

    if ((s_task_handle != NULL) ||
        s_status.running) {

        return ESP_ERR_INVALID_STATE;
    }

    BaseType_t task_result = xTaskCreate(
        signal_processing_task,
        SIGNAL_PROCESSING_TASK_NAME,
        SIGNAL_PROCESSING_TASK_STACK_SIZE,
        NULL,
        SIGNAL_PROCESSING_TASK_PRIORITY,
        &s_task_handle
    );

    if (task_result != pdPASS) {
        s_task_handle = NULL;

        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}

esp_err_t signal_processing_stop(void)
{
    if (!s_status.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    s_stop_requested = true;

    return ESP_OK;
}

esp_err_t signal_processing_get_status(
    signal_processing_status_t *output
)
{
    if (output == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!s_status.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    *output = s_status;

    return ESP_OK;
}

bool signal_processing_is_running(void)
{
    return s_status.running;
}