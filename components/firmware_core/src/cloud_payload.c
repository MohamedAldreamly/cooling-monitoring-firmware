#include "cloud_payload.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "cJSON.h"

#include "cloud_contract.h"
#include "record_contract.h"
#include "signal_registry.h"

static esp_err_t cloud_json_print(
    cJSON *root,
    char *buffer,
    size_t buffer_size
)
{
    if ((root == NULL) ||
        (buffer == NULL) ||
        (buffer_size == 0U)) {

        return ESP_ERR_INVALID_ARG;
    }

    memset(buffer, 0, buffer_size);

    const cJSON_bool result =
        cJSON_PrintPreallocated(
            root,
            buffer,
            (int)buffer_size,
            false
        );

    if (!result) {
        buffer[0] = '\0';

        return ESP_ERR_INVALID_SIZE;
    }

    return ESP_OK;
}

static esp_err_t cloud_add_common_fields(
    cJSON *root,
    const char *message_type,
    uint32_t boot_id,
    uint64_t uptime_ms,
    int64_t observed_at_ms
)
{
    if ((root == NULL) ||
        (message_type == NULL)) {

        return ESP_ERR_INVALID_ARG;
    }

    if (cJSON_AddNumberToObject(
            root,
            "schema_version",
            CLOUD_SCHEMA_VERSION
        ) == NULL) {

        return ESP_ERR_NO_MEM;
    }

    if (cJSON_AddStringToObject(
            root,
            "device_id",
            CLOUD_DEVICE_ID
        ) == NULL) {

        return ESP_ERR_NO_MEM;
    }

    if (cJSON_AddStringToObject(
            root,
            "message_type",
            message_type
        ) == NULL) {

        return ESP_ERR_NO_MEM;
    }

    if (cJSON_AddNumberToObject(
            root,
            "boot_id",
            (double)boot_id
        ) == NULL) {

        return ESP_ERR_NO_MEM;
    }

    if (cJSON_AddNumberToObject(
            root,
            "uptime_ms",
            (double)uptime_ms
        ) == NULL) {

        return ESP_ERR_NO_MEM;
    }

    /*
     * UTC is not implemented yet.
     *
     * Until Time Manager / SNTP is implemented,
     * observed_at_ms is represented as JSON null.
     */
    if (observed_at_ms > 0) {
        if (cJSON_AddNumberToObject(
                root,
                "observed_at_ms",
                (double)observed_at_ms
            ) == NULL) {

            return ESP_ERR_NO_MEM;
        }
    } else {
        if (cJSON_AddNullToObject(
                root,
                "observed_at_ms"
            ) == NULL) {

            return ESP_ERR_NO_MEM;
        }
    }

    return ESP_OK;
}

static const char *cloud_alarm_severity_string(
    alarm_severity_t severity
)
{
    switch (severity) {
        case ALARM_SEVERITY_INFO:
            return "INFO";

        case ALARM_SEVERITY_WARNING:
            return "WARNING";

        case ALARM_SEVERITY_HIGH:
            return "HIGH";

        case ALARM_SEVERITY_CRITICAL:
            return "CRITICAL";

        default:
            return "UNKNOWN";
    }
}

static const char *cloud_alarm_transition_string(
    alarm_transition_t transition
)
{
    switch (transition) {
        case ALARM_TRANSITION_PENDING:
            return "PENDING";

        case ALARM_TRANSITION_ACTIVATED:
            return "ACTIVATED";

        case ALARM_TRANSITION_RETURNED:
            return "RETURNED";

        case ALARM_TRANSITION_CLOSED:
            return "CLOSED";

        case ALARM_TRANSITION_DEGRADED:
            return "DEGRADED";

        case ALARM_TRANSITION_NONE:
        default:
            return "NONE";
    }
}

static esp_err_t cloud_add_alarm_source_value(
    cJSON *root,
    const alarm_event_t *event
)
{
    if ((root == NULL) ||
        (event == NULL)) {

        return ESP_ERR_INVALID_ARG;
    }

    if (!event->has_source_value) {
        if (cJSON_AddNullToObject(
                root,
                "source_value"
            ) == NULL) {

            return ESP_ERR_NO_MEM;
        }

        return ESP_OK;
    }

    switch (event->source_data_type) {
        case SIGNAL_DATA_TYPE_BOOL:
            if (cJSON_AddBoolToObject(
                    root,
                    "source_value",
                    event->source_value.boolean
                ) == NULL) {

                return ESP_ERR_NO_MEM;
            }
            break;

        case SIGNAL_DATA_TYPE_FLOAT32:
            if (cJSON_AddNumberToObject(
                    root,
                    "source_value",
                    (double)
                    event->source_value.float32
                ) == NULL) {

                return ESP_ERR_NO_MEM;
            }
            break;

        case SIGNAL_DATA_TYPE_INT32:
            if (cJSON_AddNumberToObject(
                    root,
                    "source_value",
                    (double)
                    event->source_value.int32
                ) == NULL) {

                return ESP_ERR_NO_MEM;
            }
            break;

        case SIGNAL_DATA_TYPE_UINT32:
            if (cJSON_AddNumberToObject(
                    root,
                    "source_value",
                    (double)
                    event->source_value.uint32
                ) == NULL) {

                return ESP_ERR_NO_MEM;
            }
            break;

        default:
            if (cJSON_AddNullToObject(
                    root,
                    "source_value"
                ) == NULL) {

                return ESP_ERR_NO_MEM;
            }

            break;
    }

    return ESP_OK;
}

static bool cloud_alarm_event_is_valid(
    const alarm_event_t *event
)
{
    if (event == NULL) {
        return false;
    }

    /*
     * alarm_code must not be empty and must contain
     * a null terminator inside its fixed-size array.
     */
    if ((event->alarm_code[0] == '\0') ||
        (memchr(
            event->alarm_code,
            '\0',
            sizeof(event->alarm_code)
        ) == NULL)) {

        return false;
    }

    if (event->alarm_instance_id == 0U) {
        return false;
    }

    if (event->transition_sequence == 0U) {
        return false;
    }

    if (event->source_signal >= SIGNAL_ID_COUNT) {
        return false;
    }

    if (event->severity >
        ALARM_SEVERITY_CRITICAL) {

        return false;
    }

    if ((event->transition ==
         ALARM_TRANSITION_NONE) ||
        (event->transition >
         ALARM_TRANSITION_DEGRADED)) {

        return false;
    }

    if (event->source_quality >
        SIGNAL_QUALITY_SUBSTITUTED) {

        return false;
    }

    if (event->has_source_value &&
        event->source_data_type >
            SIGNAL_DATA_TYPE_FLOAT32) {

        return false;
    }

    return true;
}

esp_err_t cloud_payload_encode_alarm(
    const durable_record_t *record,
    char *buffer,
    size_t buffer_size
)
{
    if ((record == NULL) ||
        (buffer == NULL) ||
        (buffer_size == 0U)) {

        return ESP_ERR_INVALID_ARG;
    }

    if (record->type != RECORD_TYPE_ALARM) {
        return ESP_ERR_INVALID_ARG;
    }

    if (record->payload_length !=
        sizeof(alarm_record_payload_v1_t)) {

        return ESP_ERR_INVALID_SIZE;
    }

    alarm_record_payload_v1_t payload;

    memcpy(
        &payload,
        record->payload,
        sizeof(payload)
    );

    if (payload.payload_version !=
        ALARM_RECORD_PAYLOAD_VERSION) {

        return ESP_ERR_INVALID_VERSION;
    }

    if (payload.payload_size !=
        sizeof(alarm_record_payload_v1_t)) {

        return ESP_ERR_INVALID_SIZE;
    }

    const alarm_event_t *event =
        &payload.alarm_event;

    if (!cloud_alarm_event_is_valid(event)) {
        return ESP_ERR_INVALID_ARG;
    }

    const char *source_signal_name =
        signal_registry_name(
            event->source_signal
        );

    if (source_signal_name == NULL) {
        return ESP_ERR_NOT_FOUND;
    }

    char record_id_string[21];
    char alarm_instance_id_string[21];

    const int record_id_length =
        snprintf(
            record_id_string,
            sizeof(record_id_string),
            "%" PRIu64,
            record->record_id
        );

    const int alarm_instance_id_length =
        snprintf(
            alarm_instance_id_string,
            sizeof(alarm_instance_id_string),
            "%" PRIu64,
            event->alarm_instance_id
        );

    if ((record_id_length < 0) ||
        ((size_t)record_id_length >=
        sizeof(record_id_string)) ||
        (alarm_instance_id_length < 0) ||
        ((size_t)alarm_instance_id_length >=
        sizeof(alarm_instance_id_string))) {

        return ESP_ERR_INVALID_SIZE;
    }

    cJSON *root = cJSON_CreateObject();

    if (root == NULL) {
        return ESP_ERR_NO_MEM;
    }

    esp_err_t result =
        cloud_add_common_fields(
            root,
            "alarm",
            record->boot_id,
            record->uptime_ms,
            record->observed_at_ms
        );

        if (result != ESP_OK) {
            cJSON_Delete(root);

            return result;
        }

        if (cJSON_AddStringToObject(
            root,
            "record_id",
            record_id_string
        ) == NULL ||

        cJSON_AddNumberToObject(
            root,
            "boot_sequence",
            (double)record->boot_sequence
        ) == NULL ||

        cJSON_AddStringToObject(
            root,
            "alarm_instance_id",
            alarm_instance_id_string
        ) == NULL ||

        cJSON_AddNumberToObject(
            root,
            "transition_sequence",
            (double)event->transition_sequence
        ) == NULL ||

        cJSON_AddStringToObject(
            root,
            "alarm_code",
            event->alarm_code
        ) == NULL ||

        cJSON_AddStringToObject(
            root,
            "severity",
            cloud_alarm_severity_string(
                event->severity
            )
        ) == NULL ||

        cJSON_AddStringToObject(
            root,
            "transition",
            cloud_alarm_transition_string(
                event->transition
               )
        ) == NULL ||

        cJSON_AddStringToObject(
            root,
            "source_signal",
            source_signal_name
        ) == NULL ||

        cJSON_AddStringToObject(
            root,
            "source_quality",
            signal_quality_to_string(
                event->source_quality
            )
        ) == NULL) {

    cJSON_Delete(root);

        return ESP_ERR_NO_MEM;
    }

    result =
        cloud_add_alarm_source_value(
            root,
            event
        );

    if (result != ESP_OK) {
        cJSON_Delete(root);

        return result;
    }

    result =
        cloud_json_print(
            root,
            buffer,
            buffer_size
        );

    cJSON_Delete(root);

    return result;
}
