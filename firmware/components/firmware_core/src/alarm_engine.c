#include "alarm_engine.h"

#include <stddef.h>
#include <string.h>

#include "alarm_engine_internal.h"
#include "app_queues.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "signal_registry.h"

static const char *TAG = "ALARM_ENGINE";

#define ALARM_ENGINE_TASK_NAME              "alarm_engine"
#define ALARM_ENGINE_TASK_STACK_SIZE        6144U
#define ALARM_ENGINE_TASK_PRIORITY          7U

#define ALARM_SNAPSHOT_TIMEOUT_MS           1000U
#define ALARM_QUEUE_TIMEOUT_MS              20U

static TaskHandle_t s_task_handle = NULL;

static alarm_engine_status_t s_status;

static bool s_stop_requested = false;

static uint64_t s_next_alarm_instance_id = 1U;

static const alarm_rule_t s_alarm_rules[] = {
    {
        .alarm_code = "HIGH_ROOM_TEMP_01",
        .source_signal = SIGNAL_ID_ROOM_TEMP_01,
        .comparison_operator = ALARM_OPERATOR_GREATER_THAN,
        .threshold = -12.0f,
        .hysteresis = 2.0f,
        .activation_delay_ms = 10000U,
        .return_delay_ms = 5000U,
        .severity = ALARM_SEVERITY_HIGH,
        .latched = false,
        .enabled = true,
    },
    {
        .alarm_code = "HIGH_ROOM_TEMP_02",
        .source_signal = SIGNAL_ID_ROOM_TEMP_02,
        .comparison_operator = ALARM_OPERATOR_GREATER_THAN,
        .threshold = -12.0f,
        .hysteresis = 2.0f,
        .activation_delay_ms = 10000U,
        .return_delay_ms = 5000U,
        .severity = ALARM_SEVERITY_HIGH,
        .latched = false,
        .enabled = true,
    },
    {
        .alarm_code = "HIGH_ROOM_TEMP_03",
        .source_signal = SIGNAL_ID_ROOM_TEMP_03,
        .comparison_operator = ALARM_OPERATOR_GREATER_THAN,
        .threshold = -12.0f,
        .hysteresis = 2.0f,
        .activation_delay_ms = 10000U,
        .return_delay_ms = 5000U,
        .severity = ALARM_SEVERITY_HIGH,
        .latched = false,
        .enabled = true,
    },
    {
        .alarm_code = "HIGH_ROOM_TEMP_04",
        .source_signal = SIGNAL_ID_ROOM_TEMP_04,
        .comparison_operator = ALARM_OPERATOR_GREATER_THAN,
        .threshold = -12.0f,
        .hysteresis = 2.0f,
        .activation_delay_ms = 10000U,
        .return_delay_ms = 5000U,
        .severity = ALARM_SEVERITY_HIGH,
        .latched = false,
        .enabled = true,
    },
    {
        .alarm_code = "DOOR_OPEN",
        .source_signal = SIGNAL_ID_DOOR_SAFE,
        .comparison_operator = ALARM_OPERATOR_BOOL_TRUE,
        .threshold = 0.0f,
        .hysteresis = 0.0f,
        .activation_delay_ms = 5000U,
        .return_delay_ms = 2000U,
        .severity = ALARM_SEVERITY_WARNING,
        .latched = false,
        .enabled = true,
    },
    {
        .alarm_code = "WATER_LEAK",
        .source_signal = SIGNAL_ID_LEAK_ALARM,
        .comparison_operator = ALARM_OPERATOR_BOOL_TRUE,
        .threshold = 0.0f,
        .hysteresis = 0.0f,
        .activation_delay_ms = 1000U,
        .return_delay_ms = 3000U,
        .severity = ALARM_SEVERITY_CRITICAL,
        .latched = false,
        .enabled = true,
    },
    {
        .alarm_code = "COMPRESSOR_TRIP",
        .source_signal = SIGNAL_ID_COMPRESSOR_TRIP,
        .comparison_operator = ALARM_OPERATOR_BOOL_TRUE,
        .threshold = 0.0f,
        .hysteresis = 0.0f,
        .activation_delay_ms = 1000U,
        .return_delay_ms = 3000U,
        .severity = ALARM_SEVERITY_CRITICAL,
        .latched = false,
        .enabled = true,
    },
    {
        .alarm_code = "POWER_FAILURE",
        .source_signal = SIGNAL_ID_POWER_FAILURE,
        .comparison_operator = ALARM_OPERATOR_BOOL_TRUE,
        .threshold = 0.0f,
        .hysteresis = 0.0f,
        .activation_delay_ms = 1000U,
        .return_delay_ms = 3000U,
        .severity = ALARM_SEVERITY_CRITICAL,
        .latched = false,
        .enabled = true,
    },
    {
        .alarm_code = "LOW_BATTERY_VOLTAGE",
        .source_signal = SIGNAL_ID_BATTERY_VOLTAGE,
        .comparison_operator = ALARM_OPERATOR_LESS_THAN,
        .threshold = 23.5f,
        .hysteresis = 0.8f,
        .activation_delay_ms = 5000U,
        .return_delay_ms = 5000U,
        .severity = ALARM_SEVERITY_HIGH,
        .latched = false,
        .enabled = true,
    },
};

static const size_t s_alarm_rule_count =
    sizeof(s_alarm_rules) /
    sizeof(s_alarm_rules[0]);

static alarm_runtime_state_t
    s_alarm_states[
        sizeof(s_alarm_rules) /
        sizeof(s_alarm_rules[0])
    ];

static const signal_value_t *alarm_find_signal(
    const signal_snapshot_t *snapshot,
    signal_id_t signal_id
)
{
    if (snapshot == NULL) {
        return NULL;
    }

    for (size_t index = 0;
         index < snapshot->signal_count;
         index++) {

        if (snapshot->signals[index].signal_id ==
            signal_id) {

            return &snapshot->signals[index];
        }
    }

    return NULL;
}

static bool alarm_quality_is_usable(
    signal_quality_t quality
)
{
    return quality == SIGNAL_QUALITY_GOOD ||
           quality == SIGNAL_QUALITY_UNCERTAIN;
}

static bool alarm_condition_float(
    const alarm_rule_t *rule,
    const alarm_runtime_state_t *runtime,
    float value
)
{
    const bool returning =
        runtime->state == ALARM_STATE_ACTIVE ||
        runtime->state == ALARM_STATE_RETURNED;

    switch (rule->comparison_operator) {
        case ALARM_OPERATOR_GREATER_THAN:
            if (returning) {
                return value >
                    (rule->threshold -
                     rule->hysteresis);
            }

            return value > rule->threshold;

        case ALARM_OPERATOR_GREATER_THAN_OR_EQUAL:
            if (returning) {
                return value >=
                    (rule->threshold -
                     rule->hysteresis);
            }

            return value >= rule->threshold;

        case ALARM_OPERATOR_LESS_THAN:
            if (returning) {
                return value <
                    (rule->threshold +
                     rule->hysteresis);
            }

            return value < rule->threshold;

        case ALARM_OPERATOR_LESS_THAN_OR_EQUAL:
            if (returning) {
                return value <=
                    (rule->threshold +
                     rule->hysteresis);
            }

            return value <= rule->threshold;

        case ALARM_OPERATOR_EQUAL:
            return value == rule->threshold;

        case ALARM_OPERATOR_NOT_EQUAL:
            return value != rule->threshold;

        default:
            return false;
    }
}

static bool alarm_condition_matches(
    const alarm_rule_t *rule,
    const alarm_runtime_state_t *runtime,
    const signal_value_t *value
)
{
    if (rule == NULL ||
        runtime == NULL ||
        value == NULL ||
        !value->has_value) {

        return false;
    }

    switch (rule->comparison_operator) {
        case ALARM_OPERATOR_BOOL_TRUE:
            return
                value->data_type ==
                    SIGNAL_DATA_TYPE_BOOL &&
                value->value.boolean;

        case ALARM_OPERATOR_BOOL_FALSE:
            return
                value->data_type ==
                    SIGNAL_DATA_TYPE_BOOL &&
                !value->value.boolean;

        default:
            break;
    }

    float numeric_value = 0.0f;

    switch (value->data_type) {
        case SIGNAL_DATA_TYPE_FLOAT32:
            numeric_value = value->value.float32;
            break;

        case SIGNAL_DATA_TYPE_INT32:
            numeric_value =
                (float)value->value.int32;
            break;

        case SIGNAL_DATA_TYPE_UINT32:
            numeric_value =
                (float)value->value.uint32;
            break;

        default:
            return false;
    }

    return alarm_condition_float(
    rule,
    runtime,
    numeric_value
	);
}

static esp_err_t alarm_publish_transition(
    const alarm_rule_t *rule,
    alarm_runtime_state_t *runtime,
    alarm_state_t previous_state,
    alarm_state_t current_state,
    alarm_transition_t transition,
    const signal_value_t *source,
    uint64_t uptime_ms,
    int64_t observed_at_ms
)
{
    alarm_event_t event;

    memset(&event, 0, sizeof(event));

    strncpy(
        event.alarm_code,
        rule->alarm_code,
        sizeof(event.alarm_code) - 1U
    );

    event.alarm_instance_id =
        runtime->alarm_instance_id;

    event.transition_sequence =
        ++runtime->transition_sequence;

    event.source_signal =
        rule->source_signal;

    event.severity =
        rule->severity;

    event.previous_state =
        previous_state;

    event.current_state =
        current_state;

    event.transition =
        transition;

    event.uptime_ms =
        uptime_ms;

    event.observed_at_ms =
        observed_at_ms;

    if (source != NULL) {
        event.source_quality =
            source->quality;

        event.source_data_type =
            source->data_type;

        event.source_value =
            source->value;

        event.has_source_value =
            source->has_value;
    }

    esp_err_t result =
        app_queues_send_alarm_event(
            &event,
            pdMS_TO_TICKS(
                ALARM_QUEUE_TIMEOUT_MS
            )
        );

    if (result != ESP_OK) {
        s_status.alarm_queue_failures++;

        ESP_LOGW(
            TAG,
            "Alarm event queue full: code=%s transition=%d",
            rule->alarm_code,
            (int)transition
        );

        return result;
    }

    s_status.alarm_events_generated++;

    ESP_LOGI(
        TAG,
        "Alarm transition: code=%s previous=%d current=%d transition=%d instance=%llu sequence=%lu",
        rule->alarm_code,
        (int)previous_state,
        (int)current_state,
        (int)transition,
        (unsigned long long)
            runtime->alarm_instance_id,
        (unsigned long)
            runtime->transition_sequence
    );

    return ESP_OK;
}

static void alarm_update_runtime_state(
    const alarm_rule_t *rule,
    alarm_runtime_state_t *runtime,
    const signal_value_t *value,
    uint64_t now_ms,
    int64_t observed_at_ms
)
{
    if (!alarm_quality_is_usable(
            value->quality
        )) {

        s_status.quality_failures++;

        if (!runtime->quality_degraded &&
            runtime->state != ALARM_STATE_NORMAL) {

            alarm_state_t previous =
                runtime->state;

            runtime->state =
                ALARM_STATE_DEGRADED;

            runtime->quality_degraded = true;

            alarm_publish_transition(
                rule,
                runtime,
                previous,
                ALARM_STATE_DEGRADED,
                ALARM_TRANSITION_DEGRADED,
                value,
                now_ms,
                observed_at_ms
            );
        }

        return;
    }

    runtime->quality_degraded = false;

    const bool condition =
        alarm_condition_matches(
            rule,
            runtime,
            value
        );

    runtime->condition_active =
        condition;

    switch (runtime->state) {
        case ALARM_STATE_NORMAL:
            if (condition) {
                runtime->state =
                    ALARM_STATE_PENDING;

                runtime->pending_since_ms =
                    now_ms;

                runtime->alarm_instance_id =
                    s_next_alarm_instance_id++;

                runtime->transition_sequence =
                    0U;

                alarm_publish_transition(
                    rule,
                    runtime,
                    ALARM_STATE_NORMAL,
                    ALARM_STATE_PENDING,
                    ALARM_TRANSITION_PENDING,
                    value,
                    now_ms,
                    observed_at_ms
                );
            }
            break;

        case ALARM_STATE_PENDING:
            if (!condition) {
                runtime->state =
                    ALARM_STATE_NORMAL;

                runtime->pending_since_ms =
                    0U;

                runtime->alarm_instance_id =
                    0U;

                runtime->transition_sequence =
                    0U;

                break;
            }

            if ((now_ms -
                 runtime->pending_since_ms) >=
                rule->activation_delay_ms) {

                runtime->state =
                    ALARM_STATE_ACTIVE;

                alarm_publish_transition(
                    rule,
                    runtime,
                    ALARM_STATE_PENDING,
                    ALARM_STATE_ACTIVE,
                    ALARM_TRANSITION_ACTIVATED,
                    value,
                    now_ms,
                    observed_at_ms
                );
            }
            break;

        case ALARM_STATE_ACTIVE:
            if (!condition) {
                runtime->state =
                    ALARM_STATE_RETURNED;

                runtime->returned_since_ms =
                    now_ms;

                alarm_publish_transition(
                    rule,
                    runtime,
                    ALARM_STATE_ACTIVE,
                    ALARM_STATE_RETURNED,
                    ALARM_TRANSITION_RETURNED,
                    value,
                    now_ms,
                    observed_at_ms
                );
            }
            break;

        case ALARM_STATE_RETURNED:
            if (condition) {
                runtime->state =
                    ALARM_STATE_ACTIVE;

                runtime->returned_since_ms =
                    0U;

                alarm_publish_transition(
                    rule,
                    runtime,
                    ALARM_STATE_RETURNED,
                    ALARM_STATE_ACTIVE,
                    ALARM_TRANSITION_ACTIVATED,
                    value,
                    now_ms,
                    observed_at_ms
                );

                break;
            }

            if ((now_ms -
                 runtime->returned_since_ms) >=
                rule->return_delay_ms) {

                alarm_publish_transition(
                    rule,
                    runtime,
                    ALARM_STATE_RETURNED,
                    ALARM_STATE_CLOSED,
                    ALARM_TRANSITION_CLOSED,
                    value,
                    now_ms,
                    observed_at_ms
                );

                runtime->state =
                    ALARM_STATE_NORMAL;

                runtime->alarm_instance_id =
                    0U;

                runtime->transition_sequence =
                    0U;

                runtime->pending_since_ms =
                    0U;

                runtime->returned_since_ms =
                    0U;
            }
            break;

        case ALARM_STATE_DEGRADED:
            if (alarm_quality_is_usable(
                    value->quality
                )) {

                runtime->state =
                    condition
                        ? ALARM_STATE_ACTIVE
                        : ALARM_STATE_RETURNED;

                runtime->quality_degraded =
                    false;
            }
            break;

        case ALARM_STATE_CLOSED:
        default:
            runtime->state =
                ALARM_STATE_NORMAL;
            break;
    }
}

esp_err_t alarm_engine_evaluate(
    const signal_snapshot_t *snapshot
)
{
    if (snapshot == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!s_status.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    for (size_t index = 0;
         index < s_alarm_rule_count;
         index++) {

        const alarm_rule_t *rule =
            &s_alarm_rules[index];

        alarm_runtime_state_t *runtime =
            &s_alarm_states[index];

        if (!rule->enabled) {
            continue;
        }

        const signal_value_t *value =
            alarm_find_signal(
                snapshot,
                rule->source_signal
            );

        if (value == NULL) {
            continue;
        }

        alarm_update_runtime_state(
            rule,
            runtime,
            value,
            snapshot->created_uptime_ms,
            snapshot->observed_at_ms
        );

        s_status.rules_evaluated++;
    }

    uint32_t active_count = 0U;
    uint32_t degraded_count = 0U;

    for (size_t index = 0;
         index < s_alarm_rule_count;
         index++) {

        if (s_alarm_states[index].state ==
            ALARM_STATE_ACTIVE) {

            active_count++;
        }

        if (s_alarm_states[index].state ==
            ALARM_STATE_DEGRADED) {

            degraded_count++;
        }
    }

    s_status.active_alarms =
        active_count;

    s_status.degraded_alarms =
        degraded_count;

    s_status.snapshots_evaluated++;

    return ESP_OK;
}

static void alarm_engine_task(
    void *argument
)
{
    (void)argument;

    signal_snapshot_t snapshot;

    s_status.running = true;
    s_stop_requested = false;

    ESP_LOGI(
        TAG,
        "Alarm Engine task started"
    );

    while (!s_stop_requested) {
        esp_err_t result =
            app_queues_receive_signal_snapshot(
                &snapshot,
                pdMS_TO_TICKS(
                    ALARM_SNAPSHOT_TIMEOUT_MS
                )
            );

        if (result == ESP_ERR_TIMEOUT) {
            continue;
        }

        if (result != ESP_OK) {
            ESP_LOGW(
                TAG,
                "Snapshot receive failed: %s",
                esp_err_to_name(result)
            );

            continue;
        }

        s_status.snapshots_received++;

        esp_err_t evaluate_result =
            alarm_engine_evaluate(
                &snapshot
            );

        if (evaluate_result != ESP_OK) {
            ESP_LOGW(
                TAG,
                "Snapshot evaluation failed: %s",
                esp_err_to_name(
                    evaluate_result
                )
            );
        }
    }

    s_status.running = false;
    s_task_handle = NULL;

    ESP_LOGI(
        TAG,
        "Alarm Engine task stopped"
    );

    vTaskDelete(NULL);
}

esp_err_t alarm_engine_init(void)
{
    if (s_status.running) {
        return ESP_ERR_INVALID_STATE;
    }

    memset(&s_status, 0, sizeof(s_status));
    memset(
        s_alarm_states,
        0,
        sizeof(s_alarm_states)
    );

    for (size_t index = 0;
         index < s_alarm_rule_count;
         index++) {

        s_alarm_states[index].state =
            ALARM_STATE_NORMAL;
    }

    s_status.initialized = true;
    s_stop_requested = false;
    s_next_alarm_instance_id = 1U;

    ESP_LOGI(
        TAG,
        "Alarm Engine initialized with %u rules",
        (unsigned int)s_alarm_rule_count
    );

    return ESP_OK;
}

esp_err_t alarm_engine_start(void)
{
    if (!s_status.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (!app_queues_is_initialized()) {
        return ESP_ERR_INVALID_STATE;
    }

    if (s_task_handle != NULL ||
        s_status.running) {

        return ESP_ERR_INVALID_STATE;
    }

    BaseType_t result = xTaskCreate(
        alarm_engine_task,
        ALARM_ENGINE_TASK_NAME,
        ALARM_ENGINE_TASK_STACK_SIZE,
        NULL,
        ALARM_ENGINE_TASK_PRIORITY,
        &s_task_handle
    );

    if (result != pdPASS) {
        s_task_handle = NULL;

        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}

esp_err_t alarm_engine_stop(void)
{
    if (!s_status.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    s_stop_requested = true;

    return ESP_OK;
}

size_t alarm_engine_rule_count(void)
{
    return s_alarm_rule_count;
}

esp_err_t alarm_engine_get_rule(
    size_t index,
    const alarm_rule_t **output
)
{
    if (output == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (index >= s_alarm_rule_count) {
        *output = NULL;

        return ESP_ERR_NOT_FOUND;
    }

    *output = &s_alarm_rules[index];

    return ESP_OK;
}

esp_err_t alarm_engine_get_status(
    alarm_engine_status_t *output
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

bool alarm_engine_is_running(void)
{
    return s_status.running;
}