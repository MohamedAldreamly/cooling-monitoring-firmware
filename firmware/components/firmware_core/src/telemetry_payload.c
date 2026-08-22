#include "telemetry_payload.h"

#include <string.h>

#include "cJSON.h"
#include "cloud_contract.h"

static cJSON *telemetry_payload_create_value(
    const signal_value_t *signal
)
{
    if ((signal == NULL) || !signal->has_value) {
        return cJSON_CreateNull();
    }

    switch (signal->data_type) {
        case SIGNAL_DATA_TYPE_BOOL:
            return cJSON_CreateBool(signal->value.boolean);

        case SIGNAL_DATA_TYPE_INT32:
            return cJSON_CreateNumber(
                (double)signal->value.int32
            );

        case SIGNAL_DATA_TYPE_UINT32:
            return cJSON_CreateNumber(
                (double)signal->value.uint32
            );

        case SIGNAL_DATA_TYPE_FLOAT32:
            return cJSON_CreateNumber(
                (double)signal->value.float32
            );

        default:
            return NULL;
    }
}

esp_err_t telemetry_payload_encode_snapshot(
    const signal_snapshot_t *snapshot,
    char *output,
    size_t output_size
)
{
    if ((snapshot == NULL) ||
        (output == NULL) ||
        (output_size == 0U) ||
        (snapshot->signal_count >
         APP_MAX_SIGNALS_PER_SNAPSHOT)) {

        return ESP_ERR_INVALID_ARG;
    }

    output[0] = '\0';

    cJSON *root = cJSON_CreateObject();
    cJSON *signals = cJSON_CreateArray();
    cJSON *quality_summary = cJSON_CreateObject();

    if ((root == NULL) ||
        (signals == NULL) ||
        (quality_summary == NULL)) {

        cJSON_Delete(root);
        cJSON_Delete(signals);
        cJSON_Delete(quality_summary);
        return ESP_ERR_NO_MEM;
    }

    cJSON_AddNumberToObject(
        root,
        "schema_version",
        CLOUD_SCHEMA_VERSION
    );

    cJSON_AddStringToObject(
        root,
        "message_type",
        "telemetry"
    );

    cJSON_AddStringToObject(
        root,
        "device_id",
        CLOUD_DEVICE_ID
    );

    cJSON_AddStringToObject(
        root,
        "firmware_version",
        CLOUD_FIRMWARE_VERSION
    );

    cJSON_AddNumberToObject(
        root,
        "snapshot_sequence",
        snapshot->snapshot_sequence
    );

    cJSON_AddNumberToObject(
        root,
        "created_uptime_ms",
        (double)snapshot->created_uptime_ms
    );

    if (snapshot->observed_at_ms > 0) {
        cJSON_AddNumberToObject(
            root,
            "observed_at_ms",
            (double)snapshot->observed_at_ms
        );
    } else {
        cJSON_AddNullToObject(
            root,
            "observed_at_ms"
        );
    }

    cJSON_AddNumberToObject(
        quality_summary,
        "good",
        snapshot->good_count
    );

    cJSON_AddNumberToObject(
        quality_summary,
        "uncertain",
        snapshot->uncertain_count
    );

    cJSON_AddNumberToObject(
        quality_summary,
        "fault",
        snapshot->fault_count
    );

    cJSON_AddNumberToObject(
        quality_summary,
        "stale",
        snapshot->stale_count
    );

    cJSON_AddNumberToObject(
        quality_summary,
        "unavailable",
        snapshot->unavailable_count
    );

    cJSON_AddItemToObject(
        root,
        "quality_summary",
        quality_summary
    );

    for (size_t index = 0U;
         index < snapshot->signal_count;
         index++) {

        const signal_value_t *signal =
            &snapshot->signals[index];

        cJSON *item = cJSON_CreateObject();
        cJSON *value =
            telemetry_payload_create_value(signal);

        if ((item == NULL) || (value == NULL)) {
            cJSON_Delete(item);
            cJSON_Delete(value);
            cJSON_Delete(signals);
            cJSON_Delete(root);
            return ESP_ERR_NO_MEM;
        }

        cJSON_AddNumberToObject(
            item,
            "signal_id",
            signal->signal_id
        );

        cJSON_AddStringToObject(
            item,
            "name",
            signal_id_to_string(signal->signal_id)
        );

        cJSON_AddItemToObject(
            item,
            "value",
            value
        );

        cJSON_AddStringToObject(
            item,
            "unit",
            signal_unit_to_string(signal->unit)
        );

        cJSON_AddStringToObject(
            item,
            "quality",
            signal_quality_to_string(signal->quality)
        );

        cJSON_AddBoolToObject(
            item,
            "has_value",
            signal->has_value
        );

        cJSON_AddNumberToObject(
            item,
            "sample_uptime_ms",
            (double)signal->uptime_ms
        );

        cJSON_AddItemToArray(
            signals,
            item
        );
    }

    cJSON_AddItemToObject(
        root,
        "signals",
        signals
    );

    const cJSON_bool printed =
        cJSON_PrintPreallocated(
            root,
            output,
            (int)output_size,
            false
        );

    cJSON_Delete(root);

    if (!printed) {
        memset(output, 0, output_size);
        return ESP_ERR_INVALID_SIZE;
    }

    return ESP_OK;
}