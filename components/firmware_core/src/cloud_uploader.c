#include "cloud_uploader.h"

#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "app_types.h"
#include "cJSON.h"
#include "cloud_contract.h"
#include "cloud_payload.h"
#include "cloud_transport.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "storage_manager.h"
#include "upload_cursor.h"

#include "runtime_shadow.h"

static const char *TAG = "CLOUD_UPLOADER";

#define CLOUD_UPLOADER_TASK_NAME          "cloud_uploader"
#define CLOUD_UPLOADER_TASK_STACK_SIZE    6144U
#define CLOUD_UPLOADER_TASK_PRIORITY      5U
#define CLOUD_UPLOADER_POLL_INTERVAL_MS   2000U
#define CLOUD_UPLOADER_RETRY_DELAY_MS     5000U
#define CLOUD_UPLOADER_ACK_TIMEOUT_MS     30000U

static TaskHandle_t s_task_handle = NULL;
static cloud_uploader_status_t s_status;
static bool s_stop_requested = false;

static portMUX_TYPE s_ack_lock = portMUX_INITIALIZER_UNLOCKED;
static bool s_ack_pending = false;
static uint64_t s_ack_record_id = 0U;
static TickType_t s_pending_publish_tick = 0U;

static bool parse_record_id_string(
    const char *text,
    uint64_t *output
)
{
    if ((text == NULL) || (text[0] == '\0') || (output == NULL)) {
        return false;
    }

    errno = 0;
    char *end = NULL;

    unsigned long long value =
        strtoull(text, &end, 10);

    if ((errno == ERANGE) ||
        (end == text) ||
        (end == NULL) ||
        (*end != '\0')) {
        return false;
    }

    *output = (uint64_t)value;
    return true;
}

static bool parse_application_ack(
    const char *payload,
    size_t payload_length,
    uint64_t *record_id
)
{
    if ((payload == NULL) ||
        (payload_length == 0U) ||
        (record_id == NULL)) {
        return false;
    }

    cJSON *root =
        cJSON_ParseWithLength(payload, payload_length);

    if (root == NULL) {
        return false;
    }

    const cJSON *status =
        cJSON_GetObjectItemCaseSensitive(root, "status");

    const cJSON *record =
        cJSON_GetObjectItemCaseSensitive(root, "record_id");

    bool valid = false;

    if (!cJSON_IsString(status) ||
        (status->valuestring == NULL) ||
        (strcmp(status->valuestring, "ok") != 0)) {
        cJSON_Delete(root);
        return false;
    }

    if (cJSON_IsString(record) &&
        (record->valuestring != NULL)) {

        valid =
            parse_record_id_string(
                record->valuestring,
                record_id
            );

    } else if (cJSON_IsNumber(record)) {

        double number = record->valuedouble;

        if ((number >= 0.0) &&
            (number <= (double)ULLONG_MAX)) {

            uint64_t converted =
                (uint64_t)number;

            if ((double)converted == number) {
                *record_id = converted;
                valid = true;
            }
        }
    }

    cJSON_Delete(root);
    return valid;
}

void cloud_uploader_transport_event_callback(
    const cloud_transport_event_t *event,
    void *user_context
)
{
    (void)user_context;

    if (event == NULL) {
        return;
    }

    switch (event->type) {

        case CLOUD_TRANSPORT_EVENT_CONNECTED: {

            /*
             * MQTT connection is now active.
             *
             * Runtime Shadow owns its own AWS Shadow
             * subscriptions and requests them through
             * the generic cloud transport API.
             */
            const esp_err_t shadow_result =
                runtime_shadow_on_cloud_connected();

            if (shadow_result != ESP_OK) {

                ESP_LOGW(
                    TAG,
                    "Runtime Shadow subscription setup failed: %s",
                    esp_err_to_name(shadow_result)
                );
            }

            break;
        }

        case CLOUD_TRANSPORT_EVENT_DISCONNECTED:

            runtime_shadow_on_cloud_disconnected();

            break;


        case CLOUD_TRANSPORT_EVENT_BROKER_PUBLISHED:

            s_status.broker_publish_acks++;

            break;

        case CLOUD_TRANSPORT_EVENT_APPLICATION_ACK: {

            uint64_t ack_record_id = 0U;

            if (!parse_application_ack(
                    event->payload,
                    event->payload_length,
                    &ack_record_id)) {

                s_status.application_acks_rejected++;

                ESP_LOGW(
                    TAG,
                    "Rejected malformed Application ACK"
                );

                return;
            }

            s_status.application_acks_received++;

            taskENTER_CRITICAL(&s_ack_lock);

            s_ack_record_id =
                ack_record_id;

            s_ack_pending =
                true;

            taskEXIT_CRITICAL(&s_ack_lock);

            if (s_task_handle != NULL) {
                xTaskNotifyGive(
                    s_task_handle
                );
            }

            break;
        }


        default:
            break;
    }
}

static void process_pending_ack(void)
{
    bool ack_available = false;
    uint64_t ack_record_id = 0U;

    taskENTER_CRITICAL(&s_ack_lock);

    if (s_ack_pending) {
        ack_available = true;
        ack_record_id = s_ack_record_id;
        s_ack_pending = false;
    }

    taskEXIT_CRITICAL(&s_ack_lock);

    if (!ack_available) {
        return;
    }

    if (!s_status.record_waiting_for_ack) {
        s_status.application_acks_rejected++;

        ESP_LOGW(
            TAG,
            "Ignoring ACK for record_id=%llu: no record waiting",
            (unsigned long long)ack_record_id
        );
        return;
    }

    if (ack_record_id != s_status.current_record_id) {
        s_status.application_acks_rejected++;

        ESP_LOGW(
            TAG,
            "ACK mismatch: expected=%llu received=%llu",
            (unsigned long long)s_status.current_record_id,
            (unsigned long long)ack_record_id
        );
        return;
    }

    esp_err_t result =
        upload_cursor_commit(
            s_status.current_record_id,
            s_status.current_next_offset
        );

    if (result != ESP_OK) {
        s_status.cursor_commit_failures++;

        ESP_LOGE(
            TAG,
            "Cursor commit failed: record_id=%llu next_offset=%llu error=%s",
            (unsigned long long)s_status.current_record_id,
            (unsigned long long)s_status.current_next_offset,
            esp_err_to_name(result)
        );

        return;
    }

    s_status.cursor_commits++;

    ESP_LOGI(
        TAG,
        "Application ACK accepted; cursor committed: record_id=%llu next_offset=%llu",
        (unsigned long long)s_status.current_record_id,
        (unsigned long long)s_status.current_next_offset
    );

    s_status.record_waiting_for_ack = false;
    s_status.current_mqtt_message_id = -1;
    s_pending_publish_tick = 0U;
}

static void cloud_uploader_task(void *argument)
{
    (void)argument;

    s_status.running = true;
    s_stop_requested = false;

    ESP_LOGI(
        TAG,
        "Cloud Uploader task started"
    );

    while (!s_stop_requested) {

        process_pending_ack();

        if (s_status.record_waiting_for_ack) {

            TickType_t now =
                xTaskGetTickCount();

            TickType_t timeout_ticks =
                pdMS_TO_TICKS(
                    CLOUD_UPLOADER_ACK_TIMEOUT_MS
                );

            if ((s_pending_publish_tick != 0U) &&
                ((now - s_pending_publish_tick) >=
                 timeout_ticks)) {

                s_status.ack_timeouts++;

                ESP_LOGW(
                    TAG,
                    "Application ACK timeout; retrying record_id=%llu",
                    (unsigned long long)s_status.current_record_id
                );

                s_status.record_waiting_for_ack = false;
                s_status.current_mqtt_message_id = -1;
                s_pending_publish_tick = 0U;
                continue;
            }

            (void)ulTaskNotifyTake(
                pdTRUE,
                pdMS_TO_TICKS(
                    CLOUD_UPLOADER_POLL_INTERVAL_MS
                )
            );

            continue;
        }

        if (!cloud_transport_is_ready()) {
            s_status.transport_not_ready_checks++;

            vTaskDelay(
                pdMS_TO_TICKS(
                    CLOUD_UPLOADER_POLL_INTERVAL_MS
                )
            );

            continue;
        }

        upload_cursor_status_t cursor;

        esp_err_t cursor_result =
            upload_cursor_get(&cursor);

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

        storage_record_read_result_t read_result;

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
                (unsigned long long)cursor.next_offset,
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

        if (read_result.record.type != RECORD_TYPE_ALARM) {
            s_status.unsupported_records++;

            ESP_LOGE(
                TAG,
                "Unsupported stored record: id=%llu type=%s",
                (unsigned long long)read_result.record.record_id,
                record_type_to_string(read_result.record.type)
            );

            /*
             * Do not skip it. Cursor must remain unchanged.
             */
            s_status.record_waiting_for_ack = true;
            s_pending_publish_tick = 0U;
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
                (unsigned long long)read_result.record.record_id,
                esp_err_to_name(encode_result)
            );

            vTaskDelay(
                pdMS_TO_TICKS(
                    CLOUD_UPLOADER_RETRY_DELAY_MS
                )
            );

            continue;
        }

        s_status.payloads_encoded++;

        /*
         * Mark pending before publish to close the ACK race window.
         */
        s_status.record_waiting_for_ack = true;
        s_status.current_mqtt_message_id = -1;
        s_pending_publish_tick = xTaskGetTickCount();

        int mqtt_message_id = -1;

        s_status.publish_requests++;

        esp_err_t publish_result =
            cloud_transport_publish_alarm(
                json_payload,
                &mqtt_message_id
            );

        if (publish_result != ESP_OK) {
            s_status.publish_failures++;

            s_status.record_waiting_for_ack = false;
            s_status.current_mqtt_message_id = -1;
            s_pending_publish_tick = 0U;

            ESP_LOGW(
                TAG,
                "Publish failed; record remains in journal: id=%llu error=%s",
                (unsigned long long)read_result.record.record_id,
                esp_err_to_name(publish_result)
            );

            vTaskDelay(
                pdMS_TO_TICKS(
                    CLOUD_UPLOADER_RETRY_DELAY_MS
                )
            );

            continue;
        }

        s_status.publishes_queued++;
        s_status.current_mqtt_message_id =
            mqtt_message_id;

        ESP_LOGI(
            TAG,
            "Alarm queued to AWS: record_id=%llu mqtt_msg_id=%d offset=%llu next_offset=%llu",
            (unsigned long long)read_result.record.record_id,
            mqtt_message_id,
            (unsigned long long)read_result.record_offset,
            (unsigned long long)read_result.next_offset
        );

        /*
         * No upload_cursor_commit() here.
         * Wait for:
         * {"record_id":<same id>,"status":"ok"}
         * on CLOUD_TOPIC_ALARM_ACK.
         */
    }

    s_status.running = false;
    s_task_handle = NULL;

    ESP_LOGI(
        TAG,
        "Cloud Uploader task stopped"
    );

    vTaskDelete(NULL);
}

esp_err_t cloud_uploader_init(void)
{
    if (s_status.running ||
        (s_task_handle != NULL)) {
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

    if ((result != ESP_OK) ||
        !storage_status.initialized ||
        !storage_status.mounted) {
        return ESP_ERR_INVALID_STATE;
    }

    memset(&s_status, 0, sizeof(s_status));

    taskENTER_CRITICAL(&s_ack_lock);
    s_ack_pending = false;
    s_ack_record_id = 0U;
    taskEXIT_CRITICAL(&s_ack_lock);

    s_pending_publish_tick = 0U;
    s_status.current_mqtt_message_id = -1;
    s_status.initialized = true;
    s_stop_requested = false;

    ESP_LOGI(
        TAG,
        "Cloud Uploader initialized"
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

    BaseType_t task_result =
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

    if (s_task_handle != NULL) {
        xTaskNotifyGive(s_task_handle);
    }

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
