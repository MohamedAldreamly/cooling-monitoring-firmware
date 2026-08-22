#include "device_status_publisher.h"

#include <string.h>

#include "cloud_contract.h"
#include "cloud_transport.h"
#include "device_status_payload.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "DEVICE_STATUS";

#define DEVICE_STATUS_TASK_NAME          "device_status"
#define DEVICE_STATUS_TASK_STACK_SIZE    4096U
#define DEVICE_STATUS_TASK_PRIORITY      4U

static TaskHandle_t s_task_handle = NULL;
static device_status_publisher_status_t s_status;
static bool s_stop_requested = false;

static void device_status_publisher_task(void *argument)
{
    (void)argument;

    s_status.running = true;

    ESP_LOGI(
        TAG,
        "Device Status Publisher started: interval=%u ms topic=%s",
        (unsigned)CLOUD_STATUS_INTERVAL_MS,
        CLOUD_TOPIC_STATUS
    );

    while (!s_stop_requested) {
        (void)ulTaskNotifyTake(
            pdTRUE,
            pdMS_TO_TICKS(CLOUD_STATUS_INTERVAL_MS)
        );

        if (s_stop_requested) {
            break;
        }

        s_status.cycles++;

        /*
         * Device Status is a live retained state. While offline, publishing
         * is skipped instead of creating a durable journal record.
         */
        if (!cloud_transport_is_ready()) {
            s_status.offline_skips++;
            continue;
        }

        char payload[CLOUD_STATUS_PAYLOAD_MAX_LEN];

        const esp_err_t encode_result =
            device_status_payload_encode(
                payload,
                sizeof(payload)
            );

        if (encode_result != ESP_OK) {
            s_status.encoding_failures++;

            ESP_LOGE(
                TAG,
                "Failed encoding Device Status: %s",
                esp_err_to_name(encode_result)
            );

            continue;
        }

        s_status.payloads_encoded++;
        s_status.publish_requests++;

        int mqtt_message_id = -1;

        const esp_err_t publish_result =
            cloud_transport_publish_topic(
                CLOUD_TOPIC_STATUS,
                payload,
                strlen(payload),
                CLOUD_STATUS_QOS,
                CLOUD_STATUS_RETAIN != 0,
                &mqtt_message_id
            );

        if (publish_result != ESP_OK) {
            s_status.publish_failures++;

            ESP_LOGW(
                TAG,
                "Device Status publish failed: %s",
                esp_err_to_name(publish_result)
            );

            continue;
        }

        s_status.publishes_queued++;
        s_status.last_mqtt_message_id =
            mqtt_message_id;

        ESP_LOGI(
            TAG,
            "Device Status queued: bytes=%u msg_id=%d status=online",
            (unsigned)strlen(payload),
            mqtt_message_id
        );
    }

    s_status.running = false;
    s_task_handle = NULL;

    ESP_LOGI(TAG, "Device Status Publisher stopped");

    vTaskDelete(NULL);
}

esp_err_t device_status_publisher_init(void)
{
    if (s_status.running ||
        (s_task_handle != NULL)) {

        return ESP_ERR_INVALID_STATE;
    }

    memset(&s_status, 0, sizeof(s_status));

    s_status.last_mqtt_message_id = -1;
    s_status.initialized = true;
    s_stop_requested = false;

    ESP_LOGI(TAG, "Device Status Publisher initialized");

    return ESP_OK;
}

esp_err_t device_status_publisher_start(void)
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
            device_status_publisher_task,
            DEVICE_STATUS_TASK_NAME,
            DEVICE_STATUS_TASK_STACK_SIZE,
            NULL,
            DEVICE_STATUS_TASK_PRIORITY,
            &s_task_handle
        );

    if (task_result != pdPASS) {
        s_task_handle = NULL;
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}

esp_err_t device_status_publisher_stop(void)
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

esp_err_t device_status_publisher_get_status(
    device_status_publisher_status_t *output
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

bool device_status_publisher_is_running(void)
{
    return s_status.running;
}