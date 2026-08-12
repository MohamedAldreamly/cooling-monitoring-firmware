#include "cloud_payload.h"

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

static esp_err_t cloud_add_signal_value(
    cJSON *signals,
    const signal_value_t *value
)
{
    if ((signals == NULL) ||
        (value == NULL)) {

        return ESP_ERR_INVALID_ARG;
    }

    const char *signal_name =
        signal_registry_name(
            value->signal_id
        );

    if (signal_name == NULL) {
        return ESP_ERR_NOT_FOUND;
    }

    cJSON *signal_object =
        cJSON_CreateObject();

    if (signal_object == NULL) {
        return ESP_ERR_NO_MEM;
    }

    bool success = true;

    /*
     * Invalid measurements are explicitly represented
     * as JSON null instead of fake numeric values.
     */
    if (!value->has_value) {
        if (cJSON_AddNullToObject(
                signal_object,
                "value"
            ) == NULL) {

            success = false;
        }
    } else {
        switch (value->data_type) {
            case SIGNAL_DATA_TYPE_BOOL:
                if (cJSON_AddBoolToObject(
                        signal_object,
                        "value",
                        value->value.boolean
                    ) == NULL) {

                    success = false;
                }
                break;

            case SIGNAL_DATA_TYPE_FLOAT32:
                if (cJSON_AddNumberToObject(
                        signal_object,
                        "value",
                        (double)value->value.float32
                    ) == NULL) {

                    success = false;
                }
                break;

            case SIGNAL_DATA_TYPE_INT32:
                if (cJSON_AddNumberToObject(
                        signal_object,
                        "value",
                        (double)value->value.int32
                    ) == NULL) {

                    success = false;
                }
                break;

            case SIGNAL_DATA_TYPE_UINT32:
                if (cJSON_AddNumberToObject(
                        signal_object,
                        "value",
                        (double)value->value.uint32
                    ) == NULL) {

                    success = false;
                }
                break;

            default:
                success = false;
                break;
        }
    }

    if (success) {
        if (cJSON_AddStringToObject(
                signal_object,
                "quality",
                signal_quality_to_string(
                    value->quality
                )
            ) == NULL) {

            success = false;
        }
    }

    if (success &&
        value->unit != SIGNAL_UNIT_NONE) {

        if (cJSON_AddStringToObject(
                signal_object,
                "unit",
                signal_unit_to_string(
                    value->unit
                )
            ) == NULL) {

            success = false;
        }
    }

    if (!success) {
        cJSON_Delete(signal_object);

        return ESP_ERR_NO_MEM;
    }

    cJSON_AddItemToObject(
        signals,
        signal_name,
        signal_object
    );

    return ESP_OK;
}

esp_err_t cloud_payload_encode_telemetry(
    const signal_snapshot_t *snapshot,
    uint32_t boot_id,
    char *buffer,
    size_t buffer_size
)
{
    if ((snapshot == NULL) ||
        (buffer == NULL) ||
        (buffer_size == 0U)) {

        return ESP_ERR_INVALID_ARG;
    }

    cJSON *root = cJSON_CreateObject();

    if (root == NULL) {
        return ESP_ERR_NO_MEM;
    }

    esp_err_t result =
        cloud_add_common_fields(
            root,
            "telemetry",
            boot_id,
            snapshot->created_uptime_ms,
            snapshot->observed_at_ms
        );

    if (result != ESP_OK) {
        cJSON_Delete(root);

        return result;
    }

    if (cJSON_AddNumberToObject(
            root,
            "sequence",
            (double)snapshot->snapshot_sequence
        ) == NULL) {

        cJSON_Delete(root);

        return ESP_ERR_NO_MEM;
    }

    cJSON *signals =
        cJSON_AddObjectToObject(
            root,
            "signals"
        );

    if (signals == NULL) {
        cJSON_Delete(root);

        return ESP_ERR_NO_MEM;
    }

    for (size_t index = 0U;
         index < snapshot->signal_count;
         index++) {

        result =
            cloud_add_signal_value(
                signals,
                &snapshot->signals[index]
            );

        if (result != ESP_OK) {
            cJSON_Delete(root);

            return result;
        }
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

    if (cJSON_AddNumberToObject(
            root,
            "record_id",
            (double)record->record_id
        ) == NULL ||

        cJSON_AddNumberToObject(
            root,
            "alarm_instance_id",
            (double)event->alarm_instance_id
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
            signal_registry_name(
                event->source_signal
            )
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

esp_err_t cloud_payload_encode_status(
    uint32_t boot_id,
    uint64_t uptime_ms,
    bool simulation,
    const char *status,
    char *buffer,
    size_t buffer_size
)
{
    if ((status == NULL) ||
        (buffer == NULL) ||
        (buffer_size == 0U)) {

        return ESP_ERR_INVALID_ARG;
    }

    cJSON *root = cJSON_CreateObject();

    if (root == NULL) {
        return ESP_ERR_NO_MEM;
    }

    esp_err_t result =
        cloud_add_common_fields(
            root,
            "status",
            boot_id,
            uptime_ms,
            0
        );

    if (result != ESP_OK) {
        cJSON_Delete(root);

        return result;
    }

    if (cJSON_AddStringToObject(
            root,
            "firmware_version",
            CLOUD_FIRMWARE_VERSION
        ) == NULL ||

        cJSON_AddStringToObject(
            root,
            "status",
            status
        ) == NULL ||

        cJSON_AddBoolToObject(
            root,
            "simulation",
            simulation
        ) == NULL) {

        cJSON_Delete(root);

        return ESP_ERR_NO_MEM;
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