#include "record_builder.h"

#include <stddef.h>
#include <string.h>

#include "app_queues.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "system_identity.h"

#include "cloud_contract.h"
#include "cloud_payload.h"
#include "record_contract.h"

static const char *TAG = "RECORD_BUILDER";

#define RECORD_BUILDER_TASK_NAME             "record_builder"
#define RECORD_BUILDER_TASK_STACK_SIZE       4096U
#define RECORD_BUILDER_TASK_PRIORITY         5U

#define ALARM_EVENT_RECEIVE_TIMEOUT_MS       1000U
#define RECORD_QUEUE_SEND_TIMEOUT_MS         50U

/*
 * Internal binary payload version.
 *
 * This is not the Cloud JSON schema version.
 * It describes the binary structure stored inside durable_record_t.
 */

static TaskHandle_t s_task_handle = NULL;

static record_builder_status_t s_status;

static bool s_stop_requested = false;

static bool record_builder_valid_alarm_event(
    const alarm_event_t *event
)
{
    if (event == NULL) {
        return false;
    }

    if (event->alarm_code[0] == '\0') {
        return false;
    }

    if (event->transition ==
        ALARM_TRANSITION_NONE) {

        return false;
    }

    if (event->source_signal >=
        SIGNAL_ID_COUNT) {

        return false;
    }

    if (event->alarm_instance_id == 0U) {
        return false;
    }

    if (event->transition_sequence == 0U) {
        return false;
    }

    return true;
}

static record_priority_t record_builder_alarm_priority(
    alarm_severity_t severity
)
{
    switch (severity) {
        case ALARM_SEVERITY_CRITICAL:
            return RECORD_PRIORITY_CRITICAL;

        case ALARM_SEVERITY_HIGH:
            return RECORD_PRIORITY_HIGH;

        case ALARM_SEVERITY_WARNING:
            return RECORD_PRIORITY_HIGH;

        case ALARM_SEVERITY_INFO:
        default:
            return RECORD_PRIORITY_NORMAL;
    }
}

static esp_err_t record_builder_encode_alarm_payload(
    const alarm_event_t *event,
    durable_record_t *record
)
{
    if ((event == NULL) || (record == NULL)) {
        return ESP_ERR_INVALID_ARG;
    }

    alarm_record_payload_v1_t payload;

    memset(&payload, 0, sizeof(payload));

    payload.payload_version =
        ALARM_RECORD_PAYLOAD_VERSION;

    payload.payload_size =
        (uint16_t)sizeof(payload);

    payload.alarm_event = *event;

    if (sizeof(payload) >
        APP_RECORD_PAYLOAD_MAX_LEN) {

        ESP_LOGE(
            TAG,
            "Alarm payload too large: size=%u capacity=%u",
            (unsigned int)sizeof(payload),
            (unsigned int)APP_RECORD_PAYLOAD_MAX_LEN
        );

        return ESP_ERR_INVALID_SIZE;
    }

    memcpy(
        record->payload,
        &payload,
        sizeof(payload)
    );

    record->payload_length =
        (uint16_t)sizeof(payload);

    return ESP_OK;
}

esp_err_t record_builder_build_alarm_record(
    const alarm_event_t *event,
    durable_record_t *output
)
{
    if ((event == NULL) || (output == NULL)) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!s_status.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (!record_builder_valid_alarm_event(event)) {
        s_status.invalid_events++;

        return ESP_ERR_INVALID_ARG;
    }

    memset(output, 0, sizeof(*output));

    uint64_t record_id = 0U;
    uint32_t boot_id = 0U;

    esp_err_t identity_result =
        system_identity_next_record_id(
            &record_id
        );

    if (identity_result != ESP_OK) {
        s_status.identity_failures++;

        ESP_LOGE(
            TAG,
            "Failed allocating record_id: %s",
            esp_err_to_name(identity_result)
        );

        return identity_result;
    }

    identity_result =
        system_identity_get_boot_id(
            &boot_id
        );

    if (identity_result != ESP_OK) {
        s_status.identity_failures++;

        ESP_LOGE(
            TAG,
            "Failed reading boot_id: %s",
            esp_err_to_name(identity_result)
        );

        return identity_result;
    }

    output->record_id = record_id;
    output->boot_id = boot_id;

    output->boot_sequence =
        ++s_status.current_boot_sequence;

    output->type =
        RECORD_TYPE_ALARM;

    output->priority =
        record_builder_alarm_priority(
            event->severity
        );

    output->uptime_ms =
        event->uptime_ms;

    output->observed_at_ms =
        event->observed_at_ms;

    /*
     * Alarm transitions always require Application ACK
     * before normal deletion from durable storage.
     */
    output->requires_application_ack = true;

    esp_err_t payload_result =
        record_builder_encode_alarm_payload(
            event,
            output
        );

    if (payload_result != ESP_OK) {
        s_status.payload_failures++;

        /*
         * record_id لا يرجع للخلف.
         * وجود فجوة أفضل من إعادة استخدام نفس المعرّف.
         */
        s_status.current_record_id =
            output->record_id;

        return payload_result;
    }

    s_status.current_record_id =
        output->record_id;

    s_status.records_created++;

    return ESP_OK;
}

static void record_builder_task(void *argument)
{
    (void)argument;

    alarm_event_t alarm_event;
    durable_record_t record;

    s_status.running = true;
    s_stop_requested = false;

    ESP_LOGI(
    TAG,
    "Record Builder task started: boot_id=%lu",
    	(unsigned long)s_status.current_boot_id
	);

    while (!s_stop_requested) {
        esp_err_t receive_result =
            app_queues_receive_alarm_event(
                &alarm_event,
                pdMS_TO_TICKS(
                    ALARM_EVENT_RECEIVE_TIMEOUT_MS
                )
            );

        if (receive_result == ESP_ERR_TIMEOUT) {
            continue;
        }

        if (receive_result != ESP_OK) {
            ESP_LOGW(
                TAG,
                "Alarm event receive failed: %s",
                esp_err_to_name(receive_result)
            );

            continue;
        }

        s_status.alarm_events_received++;

        esp_err_t build_result =
    record_builder_build_alarm_record(
        &alarm_event,
        &record
    );

		if (build_result != ESP_OK) {
    		ESP_LOGW(
        	TAG,
        	"Alarm record build failed: code=%s error=%s",
        	alarm_event.alarm_code,
        	esp_err_to_name(build_result)
    		);

    		continue;
		}

/*
 * Temporary Cloud Contract preview.
 *
 * Later this exact payload goes to MQTT instead of ESP_LOG.
 */
		static char cloud_payload[
	    	CLOUD_MQTT_PAYLOAD_MAX_LEN
		];

		esp_err_t cloud_result =
    		cloud_payload_encode_alarm(
        	&record,
        	cloud_payload,
        	sizeof(cloud_payload)
    		);

		if (cloud_result == ESP_OK) {
    		ESP_LOGI(
        	TAG,
        	"MQTT PREVIEW TOPIC: %s",
        	CLOUD_TOPIC_ALARM
    	);
	
    	ESP_LOGI(
        	TAG,
        	"MQTT PREVIEW PAYLOAD: %s",
        	cloud_payload
    		);
	} else {
    	ESP_LOGW(
        	TAG,
        	"Failed encoding alarm MQTT payload: %s",
        	esp_err_to_name(cloud_result)
    		);
	}

		esp_err_t queue_result =
    		app_queues_send_record(
        	&record,
        	pdMS_TO_TICKS(
            	RECORD_QUEUE_SEND_TIMEOUT_MS
        		)
    		);

        if (queue_result != ESP_OK) {
            s_status.record_queue_failures++;

            ESP_LOGW(
                TAG,
                "Record queue full: record_id=%llu",
                (unsigned long long)record.record_id
            );

            continue;
        }

        s_status.records_published++;

        ESP_LOGI(
            TAG,
            "Alarm record created: record_id=%llu code=%s priority=%d",
            (unsigned long long)record.record_id,
            alarm_event.alarm_code,
            (int)record.priority
        );
    }

    s_status.running = false;
    s_task_handle = NULL;

    ESP_LOGI(
        TAG,
        "Record Builder task stopped"
    );

    vTaskDelete(NULL);
}

esp_err_t record_builder_init(void)
{
    if (s_status.running) {
        return ESP_ERR_INVALID_STATE;
    }

    if (!system_identity_is_initialized()) {
        return ESP_ERR_INVALID_STATE;
    }

    memset(&s_status, 0, sizeof(s_status));

    uint32_t boot_id = 0U;

    esp_err_t result =
        system_identity_get_boot_id(
            &boot_id
        );

    if (result != ESP_OK) {
        return result;
    }

    s_stop_requested = false;

    s_status.current_boot_id =
        boot_id;

    s_status.initialized = true;

    ESP_LOGI(
        TAG,
        "Record Builder initialized: boot_id=%lu",
        (unsigned long)boot_id
    );

    return ESP_OK;
}

esp_err_t record_builder_start(void)
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

    BaseType_t result = xTaskCreate(
        record_builder_task,
        RECORD_BUILDER_TASK_NAME,
        RECORD_BUILDER_TASK_STACK_SIZE,
        NULL,
        RECORD_BUILDER_TASK_PRIORITY,
        &s_task_handle
    );

    if (result != pdPASS) {
        s_task_handle = NULL;

        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}

esp_err_t record_builder_stop(void)
{
    if (!s_status.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    s_stop_requested = true;

    return ESP_OK;
}

esp_err_t record_builder_get_status(
    record_builder_status_t *output
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

bool record_builder_is_running(void)
{
    return s_status.running;
}
