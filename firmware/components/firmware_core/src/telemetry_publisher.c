#include "telemetry_publisher.h"

#include <string.h>

#include "cloud_contract.h"
#include "cloud_transport.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "signal_snapshot_manager.h"
#include "telemetry_payload.h"

static const char *TAG = "TELEMETRY";

#define TELEMETRY_TASK_NAME          "telemetry"
#define TELEMETRY_TASK_STACK_SIZE    8192U
#define TELEMETRY_TASK_PRIORITY      4U

static TaskHandle_t s_task_handle = NULL;
static telemetry_publisher_status_t s_status;
static bool s_stop_requested = false;

static void telemetry_publisher_task(void *argument)
{
    (void)argument;

    s_status.running = true;

    ESP_LOGI(
        TAG,
        "Telemetry Publisher started: interval=%u ms topic=%s",
        (unsigned)CLOUD_TELEMETRY_INTERVAL_MS,
        CLOUD_TOPIC_TELEMETRY
    );

    while (!s_stop_requested) {
        (void)ulTaskNotifyTake(
            pdTRUE,
            pdMS_TO_TICKS(CLOUD_TELEMETRY_INTERVAL_MS)
        );

        if (s_stop_requested) {
            break;
        }

        s_status.cycles++;

        /*
         * Live telemetry is best-effort. While offline we do not create a
         * durable record and do not grow journal.bin. The next online cycle
         * publishes a fresh snapshot instead.
         */
        if (!cloud_transport_is_ready()) {
            s_status.offline_skips++;
            continue;
        }

        const uint64_t now_uptime_ms =
            (uint64_t)(esp_timer_get_time() / 1000LL);

        signal_snapshot_t snapshot;

        const esp_err_t snapshot_result =
            signal_snapshot_manager_build(
                now_uptime_ms,
                &snapshot
            );

        if (snapshot_result != ESP_OK) {
            s_status.snapshot_failures++;

            ESP_LOGW(
                TAG,
                "Failed building telemetry snapshot: %s",
                esp_err_to_name(snapshot_result)
            );

            continue;
        }

        s_status.snapshots_built++;
        s_status.last_snapshot_sequence =
            snapshot.snapshot_sequence;

        char payload[CLOUD_MQTT_PAYLOAD_MAX_LEN];

        const esp_err_t encode_result =
            telemetry_payload_encode_snapshot(
                &snapshot,
                payload,
                sizeof(payload)
            );

        if (encode_result != ESP_OK) {
            s_status.encoding_failures++;

            ESP_LOGE(
                TAG,
                "Failed encoding telemetry snapshot=%lu: %s",
                (unsigned long)snapshot.snapshot_sequence,
                esp_err_to_name(encode_result)
            );

            continue;
        }

        s_status.payloads_encoded++;
        s_status.publish_requests++;

        int mqtt_message_id = -1;

        const esp_err_t publish_result =
            cloud_transport_publish_topic(
                CLOUD_TOPIC_TELEMETRY,
                payload,
                strlen(payload),
                CLOUD_TELEMETRY_QOS,
                CLOUD_TELEMETRY_RETAIN != 0,
                &mqtt_message_id
            );

        if (publish_result != ESP_OK) {
            s_status.publish_failures++;

            ESP_LOGW(
                TAG,
                "Telemetry publish failed: snapshot=%lu error=%s",
                (unsigned long)snapshot.snapshot_sequence,
                esp_err_to_name(publish_result)
            );

            continue;
        }

        s_status.publishes_queued++;
        s_status.last_mqtt_message_id =
            mqtt_message_id;

        ESP_LOGI(
            TAG,
            "Telemetry queued: snapshot=%lu signals=%u bytes=%u msg_id=%d",
            (unsigned long)snapshot.snapshot_sequence,
            (unsigned)snapshot.signal_count,
            (unsigned)strlen(payload),
            mqtt_message_id
        );
    }

    s_status.running = false;
    s_task_handle = NULL;

    ESP_LOGI(TAG, "Telemetry Publisher stopped");

    vTaskDelete(NULL);
}

esp_err_t telemetry_publisher_init(void)
{
    if (s_status.running ||
        (s_task_handle != NULL)) {

        return ESP_ERR_INVALID_STATE;
    }

    memset(&s_status, 0, sizeof(s_status));

    s_status.last_mqtt_message_id = -1;
    s_status.initialized = true;
    s_stop_requested = false;

    ESP_LOGI(TAG, "Telemetry Publisher initialized");

    return ESP_OK;
}

esp_err_t telemetry_publisher_start(void)
{
    if (!s_status.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (s_status.running ||
        (s_task_handle != NULL)) {

        return ESP_ERR_INVALID_STATE;
    }

    s_stop_requested = false;

    const BaseType_t task_result =
        xTaskCreate(
            telemetry_publisher_task,
            TELEMETRY_TASK_NAME,
            TELEMETRY_TASK_STACK_SIZE,
            NULL,
            TELEMETRY_TASK_PRIORITY,
            &s_task_handle
        );

    if (task_result != pdPASS) {
        s_task_handle = NULL;
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}

esp_err_t telemetry_publisher_stop(void)
{
    if (!s_status.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (!s_status.running &&
        (s_task_handle == NULL)) {

        return ESP_ERR_INVALID_STATE;
    }

    s_stop_requested = true;

    if (s_task_handle != NULL) {
        xTaskNotifyGive(s_task_handle);
    }

    return ESP_OK;
}

esp_err_t telemetry_publisher_get_status(
    telemetry_publisher_status_t *output
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

bool telemetry_publisher_is_running(void)
{
    return s_status.running;
}