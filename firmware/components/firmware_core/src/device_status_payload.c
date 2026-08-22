#include "device_status_payload.h"

#include <string.h>

#include "cJSON.h"
#include "cloud_contract.h"
#include "cloud_transport.h"
#include "esp_timer.h"
#include "storage_manager.h"
#include "upload_cursor.h"
#include "wifi_manager.h"

esp_err_t device_status_payload_encode(
    char *output,
    size_t output_size
)
{
    if ((output == NULL) || (output_size == 0U)) {
        return ESP_ERR_INVALID_ARG;
    }

    output[0] = '\0';

    storage_manager_status_t storage;
    upload_cursor_status_t cursor;
    cloud_transport_status_t transport;
    wifi_manager_status_t wifi;

    const esp_err_t storage_result =
        storage_manager_get_status(&storage);

    const esp_err_t cursor_result =
        upload_cursor_get(&cursor);

    const esp_err_t transport_result =
        cloud_transport_get_status(&transport);

    const esp_err_t wifi_result =
        wifi_manager_get_status(&wifi);

    if ((storage_result != ESP_OK) ||
        (cursor_result != ESP_OK) ||
        (transport_result != ESP_OK) ||
        (wifi_result != ESP_OK)) {

        return ESP_ERR_INVALID_STATE;
    }

    uint64_t pending_bytes = 0U;

    if (storage.journal_size_bytes > cursor.next_offset) {
        pending_bytes =
            storage.journal_size_bytes - cursor.next_offset;
    }

    const uint64_t uptime_ms =
        (uint64_t)(esp_timer_get_time() / 1000LL);

    const bool online =
        wifi.connected &&
        wifi.has_ip &&
        transport.connected &&
        transport.ready;

    cJSON *root = cJSON_CreateObject();

    if (root == NULL) {
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
        "device_status"
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

    cJSON_AddStringToObject(
        root,
        "status",
        online ? "online" : "degraded"
    );

    cJSON_AddNumberToObject(
        root,
        "uptime_ms",
        (double)uptime_ms
    );

    cJSON_AddBoolToObject(
        root,
        "wifi_connected",
        wifi.connected && wifi.has_ip
    );

    cJSON_AddBoolToObject(
        root,
        "mqtt_connected",
        transport.connected
    );

    cJSON_AddBoolToObject(
        root,
        "mqtt_ready",
        transport.ready
    );

    cJSON_AddBoolToObject(
        root,
        "storage_mounted",
        storage.mounted
    );

    cJSON_AddBoolToObject(
        root,
        "storage_degraded",
        storage.degraded
    );

    cJSON_AddNumberToObject(
        root,
        "journal_records_written",
        storage.records_written
    );

    cJSON_AddNumberToObject(
        root,
        "journal_size_bytes",
        (double)storage.journal_size_bytes
    );

    cJSON_AddNumberToObject(
        root,
        "cursor_offset",
        (double)cursor.next_offset
    );

    cJSON_AddNumberToObject(
        root,
        "last_acknowledged_record_id",
        (double)cursor.last_acked_record_id
    );

    cJSON_AddNumberToObject(
        root,
        "pending_bytes",
        (double)pending_bytes
    );

    cJSON_AddNumberToObject(
        root,
        "cloud_disconnections",
        transport.disconnections
    );

    cJSON_AddNumberToObject(
        root,
        "cloud_errors",
        transport.errors
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