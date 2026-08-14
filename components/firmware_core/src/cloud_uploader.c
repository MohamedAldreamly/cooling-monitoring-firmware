#include "cloud_uploader.h"

#include <string.h>

#include "app_types.h"
#include "cloud_contract.h"
#include "cloud_payload.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "storage_manager.h"
#include "upload_cursor.h"

static const char *TAG = "CLOUD_UPLOADER";

#define CLOUD_UPLOADER_TASK_NAME          "cloud_uploader"
#define CLOUD_UPLOADER_TASK_STACK_SIZE    6144U
#define CLOUD_UPLOADER_TASK_PRIORITY      5U

#define CLOUD_UPLOADER_POLL_INTERVAL_MS   2000U
#define CLOUD_UPLOADER_RETRY_DELAY_MS     5000U

static TaskHandle_t s_task_handle = NULL;

static cloud_uploader_status_t s_status;

static bool s_stop_requested = false;

static void cloud_uploader_task(
    void *argument
)
{
    (void)argument;

    s_status.running = true;
    s_stop_requested = false;

    ESP_LOGI(
        TAG,
        "Cloud Uploader preview task started"
    );

    while (!s_stop_requested) {
        /*
         * During the preview stage, do not read another
         * record after successfully preparing one.
         *
         * The cursor must remain unchanged until a real
         * Application ACK is received from AWS.
         */
        if (s_status.record_waiting_for_ack) {
            vTaskDelay(
                pdMS_TO_TICKS(
                    CLOUD_UPLOADER_POLL_INTERVAL_MS
                )
            );

            continue;
        }

        upload_cursor_status_t cursor;

        esp_err_t cursor_result =
            upload_cursor_get(
                &cursor
            );

        if (cursor_result != ESP_OK) {
            s_status.read_failures++;

            ESP_LOGE(
                TAG,
                "Failed reading upload cursor: %s",
                esp_err_to_name(cursor_result)
            );

            vTaskDelay(
                pdMS_TO_TICKS(
                    CLOUD_UPLOADER_RETRY_DELAY_MS
                )
            );

            continue;
        }

        storage_record_read_result_t
            read_result;

        esp_err_t read_status =
            storage_manager_read_record_at(
                cursor.next_offset,
                &read_result
            );

        if (read_status == ESP_ERR_NOT_FOUND) {
            s_status.empty_journal_checks++;

            vTaskDelay(
                pdMS_TO_TICKS(
                    CLOUD_UPLOADER_POLL_INTERVAL_MS
                )
            );

            continue;
        }

        if (read_status != ESP_OK) {
            s_status.read_failures++;

            ESP_LOGE(
                TAG,
                "Journal read failed: offset=%llu error=%s",
                (unsigned long long)
                    cursor.next_offset,
                esp_err_to_name(read_status)
            );

            vTaskDelay(
                pdMS_TO_TICKS(
                    CLOUD_UPLOADER_RETRY_DELAY_MS
                )
            );

            continue;
        }

        s_status.records_read++;

        s_status.current_record_id =
            read_result.record.record_id;

        s_status.current_record_offset =
            read_result.record_offset;

        s_status.current_next_offset =
            read_result.next_offset;

        if (read_result.record.type !=
            RECORD_TYPE_ALARM) {

            s_status.unsupported_records++;

            ESP_LOGE(
                TAG,
                "Unsupported stored record: id=%llu type=%s",
                (unsigned long long)
                    read_result.record.record_id,
                record_type_to_string(
                    read_result.record.type
                )
            );

            /*
             * Do not skip an unsupported record silently.
             * Advancing the cursor here could lose data.
             */
            s_status.record_waiting_for_ack =
                true;

            continue;
        }

        char json_payload[
            CLOUD_ALARM_PAYLOAD_MAX_LEN
        ];

        esp_err_t encode_result =
            cloud_payload_encode_alarm(
                &read_result.record,
                json_payload,
                sizeof(json_payload)
            );

        if (encode_result != ESP_OK) {
            s_status.encoding_failures++;

            ESP_LOGE(
                TAG,
                "Alarm JSON encoding failed: id=%llu error=%s",
                (unsigned long long)
                    read_result.record.record_id,
                esp_err_to_name(encode_result)
            );

            /*
             * Keep retrying without advancing the cursor.
             */
            vTaskDelay(
                pdMS_TO_TICKS(
                    CLOUD_UPLOADER_RETRY_DELAY_MS
                )
            );

            continue;
        }

        s_status.payloads_encoded++;

        /*
         * Preview only.
         *
         * This JSON will later be passed to cloud_transport.
         * No upload_cursor_commit() is allowed here.
         */
        ESP_LOGI(
            TAG,
            "UPLOAD PREVIEW: record_id=%llu offset=%llu next_offset=%llu",
            (unsigned long long)
                read_result.record.record_id,
            (unsigned long long)
                read_result.record_offset,
            (unsigned long long)
                read_result.next_offset
        );

        ESP_LOGI(
            TAG,
            "UPLOAD PREVIEW TOPIC: %s",
            CLOUD_TOPIC_ALARM
        );

        ESP_LOGI(
            TAG,
            "UPLOAD PREVIEW PAYLOAD: %s",
            json_payload
        );

        s_status.record_waiting_for_ack =
            true;
    }

    s_status.running = false;
    s_task_handle = NULL;

    ESP_LOGI(
        TAG,
        "Cloud Uploader preview task stopped"
    );

    vTaskDelete(NULL);
}

esp_err_t cloud_uploader_init(void)
{
    if (s_status.running ||
        s_task_handle != NULL) {

        return ESP_ERR_INVALID_STATE;
    }

    if (!upload_cursor_is_initialized()) {
        return ESP_ERR_INVALID_STATE;
    }

    storage_manager_status_t storage_status;

    esp_err_t result =
        storage_manager_get_status(
            &storage_status
        );

    if (result != ESP_OK ||
        !storage_status.initialized ||
        !storage_status.mounted) {

        return ESP_ERR_INVALID_STATE;
    }

    memset(&s_status, 0, sizeof(s_status));

    s_status.initialized = true;
    s_stop_requested = false;

    ESP_LOGI(
        TAG,
        "Cloud Uploader initialized in preview mode"
    );

    return ESP_OK;
}

esp_err_t cloud_uploader_start(void)
{
    if (!s_status.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if ((s_task_handle != NULL) ||
        s_status.running) {

        return ESP_ERR_INVALID_STATE;
    }

    s_stop_requested = false;

    const BaseType_t task_result =
        xTaskCreate(
            cloud_uploader_task,
            CLOUD_UPLOADER_TASK_NAME,
            CLOUD_UPLOADER_TASK_STACK_SIZE,
            NULL,
            CLOUD_UPLOADER_TASK_PRIORITY,
            &s_task_handle
        );

    if (task_result != pdPASS) {
        s_task_handle = NULL;

        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}

esp_err_t cloud_uploader_stop(void)
{
    if (!s_status.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if ((s_task_handle == NULL) &&
        !s_status.running) {

        return ESP_ERR_INVALID_STATE;
    }

    s_stop_requested = true;

    return ESP_OK;
}

esp_err_t cloud_uploader_get_status(
    cloud_uploader_status_t *output
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

bool cloud_uploader_is_running(void)
{
    return s_status.running;
}