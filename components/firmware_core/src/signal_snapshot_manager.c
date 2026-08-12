#include "signal_snapshot_manager.h"

#include <stddef.h>
#include <string.h>

#include "app_queues.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "signal_registry.h"
#include "signal_snapshot_manager_internal.h"

#include "cloud_contract.h"
#include "cloud_payload.h"
#include "system_identity.h"

static const char *TAG = "SNAPSHOT_MANAGER";

static uint64_t s_last_cloud_telemetry_ms = 0U;

#define SNAPSHOT_MANAGER_TASK_NAME              "snapshot_manager"
#define SNAPSHOT_MANAGER_TASK_STACK_SIZE        5120U
#define SNAPSHOT_MANAGER_TASK_PRIORITY          5U

#define SNAPSHOT_RECEIVE_TIMEOUT_MS             100U
#define SNAPSHOT_PUBLISH_INTERVAL_MS            1000U
#define SNAPSHOT_QUEUE_TIMEOUT_MS               20U

static TaskHandle_t s_task_handle = NULL;
static SemaphoreHandle_t s_latest_values_mutex = NULL;

static latest_signal_entry_t
    s_latest_entries[SIGNAL_ID_COUNT];

static signal_snapshot_manager_status_t s_status;

static bool s_stop_requested = false;

static void snapshot_preview_cloud_telemetry(
    const signal_snapshot_t *snapshot
);

static uint64_t snapshot_now_ms(void)
{
    return (uint64_t)(
        esp_timer_get_time() / 1000
    );
}

static bool snapshot_valid_signal_id(
    signal_id_t signal_id
)
{
    return signal_id < SIGNAL_ID_COUNT;
}

static void snapshot_initialize_entries(void)
{
    memset(
        s_latest_entries,
        0,
        sizeof(s_latest_entries)
    );

    for (signal_id_t id = 0;
         id < SIGNAL_ID_COUNT;
         id++) {

        latest_signal_entry_t *entry =
            &s_latest_entries[id];

        entry->value.signal_id = id;
        entry->value.quality =
            SIGNAL_QUALITY_INITIALIZING;

        entry->value.has_value = false;
        entry->received_once = false;
        entry->stale_reported = false;
    }
}

static esp_err_t snapshot_update_latest(
    const signal_value_t *value
)
{
    if (value == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!snapshot_valid_signal_id(
            value->signal_id
        )) {

        s_status.invalid_signal_ids++;

        return ESP_ERR_INVALID_ARG;
    }

    if (xSemaphoreTake(
            s_latest_values_mutex,
            pdMS_TO_TICKS(20)
        ) != pdTRUE) {

        return ESP_ERR_TIMEOUT;
    }

    latest_signal_entry_t *entry =
        &s_latest_entries[value->signal_id];

    entry->value = *value;
    entry->last_update_uptime_ms =
        value->uptime_ms;

    entry->received_once = true;
    entry->stale_reported = false;

    xSemaphoreGive(s_latest_values_mutex);

    s_status.values_received++;

    return ESP_OK;
}

static void snapshot_count_quality(
    signal_snapshot_t *snapshot,
    signal_quality_t quality
)
{
    switch (quality) {
        case SIGNAL_QUALITY_GOOD:
            snapshot->good_count++;
            break;

        case SIGNAL_QUALITY_UNCERTAIN:
        case SIGNAL_QUALITY_INITIALIZING:
        case SIGNAL_QUALITY_SUBSTITUTED:
            snapshot->uncertain_count++;
            break;

        case SIGNAL_QUALITY_STALE:
            snapshot->stale_count++;
            break;

        case SIGNAL_QUALITY_NOT_AVAILABLE:
            snapshot->unavailable_count++;
            break;

        case SIGNAL_QUALITY_BAD:
        case SIGNAL_QUALITY_OUT_OF_RANGE:
        case SIGNAL_QUALITY_SENSOR_FAULT:
        case SIGNAL_QUALITY_COMMUNICATION_FAULT:
            snapshot->fault_count++;
            break;

        default:
            snapshot->fault_count++;
            break;
    }
}

static void snapshot_apply_stale_policy(
    signal_id_t signal_id,
    uint64_t now_uptime_ms,
    latest_signal_entry_t *entry,
    signal_value_t *output
)
{
    const signal_definition_t *definition = NULL;

    if (signal_registry_get(
            signal_id,
            &definition
        ) != ESP_OK ||
        definition == NULL) {

        output->quality =
            SIGNAL_QUALITY_NOT_AVAILABLE;

        output->has_value = false;

        return;
    }

    if (!entry->received_once) {
        output->quality =
            SIGNAL_QUALITY_INITIALIZING;

        output->has_value = false;

        return;
    }

    const uint64_t age_ms =
        now_uptime_ms -
        entry->last_update_uptime_ms;

    if (age_ms > definition->stale_after_ms) {
        output->quality =
            SIGNAL_QUALITY_STALE;

        /*
         * نحافظ على آخر قيمة مع Quality=STALE.
         * هذا مختلف عن communication fault الذي لا يحمل قيمة.
         */
        output->has_value =
            entry->value.has_value;

        if (!entry->stale_reported) {
            entry->stale_reported = true;
            s_status.stale_transitions++;

            ESP_LOGW(
                TAG,
                "Signal became stale: signal=%s age=%llu ms",
                definition->name,
                (unsigned long long)age_ms
            );
        }
    }
}

esp_err_t signal_snapshot_manager_build(
    uint64_t now_uptime_ms,
    signal_snapshot_t *output
)
{
    if (output == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!s_status.initialized ||
        s_latest_values_mutex == NULL) {

        return ESP_ERR_INVALID_STATE;
    }

    memset(output, 0, sizeof(*output));

    output->snapshot_sequence =
        ++s_status.current_snapshot_sequence;

    output->created_uptime_ms =
        now_uptime_ms;

    /*
     * UTC سيبقى صفرًا حاليًا حتى تنفيذ time_manager.
     */
    output->observed_at_ms = 0;

    if (xSemaphoreTake(
            s_latest_values_mutex,
            pdMS_TO_TICKS(50)
        ) != pdTRUE) {

        return ESP_ERR_TIMEOUT;
    }

    size_t output_index = 0;

    for (signal_id_t id = 0;
         id < SIGNAL_ID_COUNT;
         id++) {

        if (output_index >=
            APP_MAX_SIGNALS_PER_SNAPSHOT) {

            break;
        }

        const signal_definition_t *definition = NULL;

        if (signal_registry_get(
                id,
                &definition
            ) != ESP_OK ||
            definition == NULL ||
            !definition->enabled) {

            continue;
        }

        latest_signal_entry_t *entry =
            &s_latest_entries[id];

        signal_value_t value;

        if (entry->received_once) {
            value = entry->value;
        } else {
            memset(&value, 0, sizeof(value));

            value.signal_id = id;
            value.data_type =
                definition->data_type;

            value.unit = definition->unit;
            value.quality =
                SIGNAL_QUALITY_INITIALIZING;

            value.has_value = false;
            value.uptime_ms =
                now_uptime_ms;

            value.observed_at_ms = 0;
        }

        snapshot_apply_stale_policy(
            id,
            now_uptime_ms,
            entry,
            &value
        );

        output->signals[output_index] =
            value;

        snapshot_count_quality(
            output,
            value.quality
        );

        output_index++;
    }

    output->signal_count = output_index;

    xSemaphoreGive(s_latest_values_mutex);

    s_status.snapshots_created++;

    return ESP_OK;
}

esp_err_t signal_snapshot_manager_get_latest(
    signal_id_t signal_id,
    signal_value_t *output
)
{
    if (output == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!s_status.initialized ||
        s_latest_values_mutex == NULL) {

        return ESP_ERR_INVALID_STATE;
    }

    if (!snapshot_valid_signal_id(signal_id)) {
        return ESP_ERR_INVALID_ARG;
    }

    if (xSemaphoreTake(
            s_latest_values_mutex,
            pdMS_TO_TICKS(20)
        ) != pdTRUE) {

        return ESP_ERR_TIMEOUT;
    }

    latest_signal_entry_t *entry =
        &s_latest_entries[signal_id];

    if (!entry->received_once) {
        xSemaphoreGive(s_latest_values_mutex);

        return ESP_ERR_NOT_FOUND;
    }

    *output = entry->value;

    xSemaphoreGive(s_latest_values_mutex);

    return ESP_OK;
}

static void signal_snapshot_manager_task(
    void *argument
)
{
    (void)argument;

    signal_value_t value;
    signal_snapshot_t snapshot;

    uint64_t next_publish_ms =
        snapshot_now_ms() +
        SNAPSHOT_PUBLISH_INTERVAL_MS;

    s_status.running = true;
    s_stop_requested = false;

    ESP_LOGI(
        TAG,
        "Signal Snapshot Manager task started"
    );

    while (!s_stop_requested) {
        esp_err_t receive_result =
            app_queues_receive_signal_value(
                &value,
                pdMS_TO_TICKS(
                    SNAPSHOT_RECEIVE_TIMEOUT_MS
                )
            );

        if (receive_result == ESP_OK) {
            esp_err_t update_result =
                snapshot_update_latest(&value);

            if (update_result != ESP_OK) {
                ESP_LOGW(
                    TAG,
                    "Failed updating latest signal: %s",
                    esp_err_to_name(update_result)
                );
            }
        } else if (
            receive_result != ESP_ERR_TIMEOUT
        ) {
            ESP_LOGW(
                TAG,
                "Signal value receive failed: %s",
                esp_err_to_name(receive_result)
            );
        }

        const uint64_t now_ms =
            snapshot_now_ms();

        if (now_ms < next_publish_ms) {
            continue;
        }

        esp_err_t build_result =
            signal_snapshot_manager_build(
                now_ms,
                &snapshot
            );
            
        if (build_result == ESP_OK) {
            snapshot_preview_cloud_telemetry(
                &snapshot
            );

            esp_err_t queue_result =
                app_queues_send_signal_snapshot(
                    &snapshot,
                    pdMS_TO_TICKS(
                        SNAPSHOT_QUEUE_TIMEOUT_MS
                    )
                );

            if (queue_result == ESP_OK) {
                s_status.snapshots_published++;
            } else {
                s_status.snapshot_queue_failures++;

                ESP_LOGW(
                    TAG,
                    "Snapshot queue full: sequence=%lu",
                    (unsigned long)
                    snapshot.snapshot_sequence
                );
            }
        } else {
            ESP_LOGW(
                TAG,
                "Snapshot build failed: %s",
                esp_err_to_name(build_result)
            );
        }

        do {
            next_publish_ms +=
                SNAPSHOT_PUBLISH_INTERVAL_MS;
        } while (next_publish_ms <= now_ms);
    }

    s_status.running = false;
    s_task_handle = NULL;

    ESP_LOGI(
        TAG,
        "Signal Snapshot Manager task stopped"
    );

    vTaskDelete(NULL);
}

esp_err_t signal_snapshot_manager_init(void)
{
    if (s_status.running) {
        return ESP_ERR_INVALID_STATE;
    }

    memset(&s_status, 0, sizeof(s_status));

    snapshot_initialize_entries();

    if (s_latest_values_mutex == NULL) {
        s_latest_values_mutex =
            xSemaphoreCreateMutex();

        if (s_latest_values_mutex == NULL) {
            return ESP_ERR_NO_MEM;
        }
    }

    s_status.initialized = true;
    s_stop_requested = false;

    ESP_LOGI(
        TAG,
        "Signal Snapshot Manager initialized"
    );

    return ESP_OK;
}

esp_err_t signal_snapshot_manager_start(void)
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
        signal_snapshot_manager_task,
        SNAPSHOT_MANAGER_TASK_NAME,
        SNAPSHOT_MANAGER_TASK_STACK_SIZE,
        NULL,
        SNAPSHOT_MANAGER_TASK_PRIORITY,
        &s_task_handle
    );

    if (result != pdPASS) {
        s_task_handle = NULL;

        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}

esp_err_t signal_snapshot_manager_stop(void)
{
    if (!s_status.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    s_stop_requested = true;

    return ESP_OK;
}

esp_err_t signal_snapshot_manager_get_status(
    signal_snapshot_manager_status_t *output
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

bool signal_snapshot_manager_is_running(void)
{
    return s_status.running;
}

static void snapshot_preview_cloud_telemetry(
    const signal_snapshot_t *snapshot
)
{
    if (snapshot == NULL) {
        return;
    }

    if ((snapshot->created_uptime_ms -
         s_last_cloud_telemetry_ms) <
        CLOUD_TELEMETRY_INTERVAL_MS) {

        return;
    }

    uint32_t boot_id = 0U;

    esp_err_t identity_result =
        system_identity_get_boot_id(
            &boot_id
        );

    if (identity_result != ESP_OK) {
        ESP_LOGW(
            TAG,
            "Unable to get boot_id for telemetry: %s",
            esp_err_to_name(identity_result)
        );

        return;
    }

    static char payload[
        CLOUD_MQTT_PAYLOAD_MAX_LEN
    ];

    esp_err_t result =
        cloud_payload_encode_telemetry(
            snapshot,
            boot_id,
            payload,
            sizeof(payload)
        );

    if (result != ESP_OK) {
        ESP_LOGW(
            TAG,
            "Telemetry MQTT encoding failed: %s",
            esp_err_to_name(result)
        );

        return;
    }

    s_last_cloud_telemetry_ms =
        snapshot->created_uptime_ms;

    ESP_LOGI(
        TAG,
        "MQTT PREVIEW TOPIC: %s",
        CLOUD_TOPIC_TELEMETRY
    );

    ESP_LOGI(
        TAG,
        "MQTT PREVIEW PAYLOAD: %s",
        payload
    );
}
