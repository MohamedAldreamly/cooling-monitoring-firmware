#include "upload_cursor.h"

#include <stddef.h>
#include <string.h>

#include "esp_log.h"
#include "nvs.h"

static const char *TAG = "UPLOAD_CURSOR";

#define UPLOAD_CURSOR_NVS_NAMESPACE     "cloud_upload"
#define UPLOAD_CURSOR_NVS_KEY           "cursor"

#define UPLOAD_CURSOR_MAGIC             0x43555253UL
#define UPLOAD_CURSOR_VERSION           1U

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint16_t version;
    uint16_t size;

    uint64_t next_offset;
    uint64_t last_acked_record_id;

    uint32_t crc;
} upload_cursor_blob_t;

static upload_cursor_status_t s_status;

static uint32_t upload_cursor_crc32(
    const void *data,
    size_t length
)
{
    if ((data == NULL) &&
        (length > 0U)) {

        return 0U;
    }

    const uint8_t *bytes =
        (const uint8_t *)data;

    uint32_t crc = 0xFFFFFFFFUL;

    for (size_t index = 0U;
         index < length;
         index++) {

        crc ^= bytes[index];

        for (uint8_t bit = 0U;
             bit < 8U;
             bit++) {

            const uint32_t mask =
                (uint32_t)(
                    -(int32_t)(crc & 1U)
                );

            crc =
                (crc >> 1U) ^
                (0xEDB88320UL & mask);
        }
    }

    return ~crc;
}

static uint32_t upload_cursor_blob_crc(
    const upload_cursor_blob_t *blob
)
{
    if (blob == NULL) {
        return 0U;
    }

    upload_cursor_blob_t copy = *blob;

    copy.crc = 0U;

    return upload_cursor_crc32(
        &copy,
        sizeof(copy)
    );
}

static bool upload_cursor_blob_is_valid(
    const upload_cursor_blob_t *blob
)
{
    if (blob == NULL) {
        return false;
    }

    if (blob->magic !=
        UPLOAD_CURSOR_MAGIC) {

        return false;
    }

    if (blob->version !=
        UPLOAD_CURSOR_VERSION) {

        return false;
    }

    if (blob->size !=
        sizeof(upload_cursor_blob_t)) {

        return false;
    }

    return blob->crc ==
        upload_cursor_blob_crc(blob);
}

static esp_err_t upload_cursor_read_blob(
    upload_cursor_blob_t *blob
)
{
    if (blob == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t handle;

    esp_err_t result =
        nvs_open(
            UPLOAD_CURSOR_NVS_NAMESPACE,
            NVS_READONLY,
            &handle
        );

    if (result != ESP_OK) {
        return result;
    }

    size_t blob_size = sizeof(*blob);

    result =
        nvs_get_blob(
            handle,
            UPLOAD_CURSOR_NVS_KEY,
            blob,
            &blob_size
        );

    nvs_close(handle);

    if (result != ESP_OK) {
        return result;
    }

    if (blob_size != sizeof(*blob)) {
        return ESP_ERR_INVALID_SIZE;
    }

    return ESP_OK;
}

static esp_err_t upload_cursor_write_blob(
    const upload_cursor_blob_t *blob
)
{
    if (blob == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t handle;

    esp_err_t result =
        nvs_open(
            UPLOAD_CURSOR_NVS_NAMESPACE,
            NVS_READWRITE,
            &handle
        );

    if (result != ESP_OK) {
        return result;
    }

    result =
        nvs_set_blob(
            handle,
            UPLOAD_CURSOR_NVS_KEY,
            blob,
            sizeof(*blob)
        );

    if (result == ESP_OK) {
        result = nvs_commit(handle);
    }

    nvs_close(handle);

    return result;
}

esp_err_t upload_cursor_init(void)
{
    memset(&s_status, 0, sizeof(s_status));

    upload_cursor_blob_t blob;

    memset(&blob, 0, sizeof(blob));

    esp_err_t result =
        upload_cursor_read_blob(
            &blob
        );

    if (result == ESP_ERR_NVS_NOT_FOUND ||
        result == ESP_ERR_NVS_NOT_INITIALIZED ||
        result == ESP_ERR_NVS_INVALID_HANDLE) {

        /*
         * A missing cursor is normal on first boot.
         */
        s_status.next_offset = 0U;
        s_status.last_acked_record_id = 0U;
        s_status.initialized = true;

        ESP_LOGI(
            TAG,
            "No saved cursor; starting at offset 0"
        );

        return ESP_OK;
    }

    if (result != ESP_OK) {
        ESP_LOGE(
            TAG,
            "Failed reading cursor: %s",
            esp_err_to_name(result)
        );

        return result;
    }

    if (!upload_cursor_blob_is_valid(
            &blob
        )) {

        ESP_LOGE(
            TAG,
            "Saved upload cursor is invalid"
        );

        return ESP_ERR_INVALID_CRC;
    }

    s_status.next_offset =
        blob.next_offset;

    s_status.last_acked_record_id =
        blob.last_acked_record_id;

    s_status.initialized = true;

    ESP_LOGI(
        TAG,
        "Cursor restored: offset=%llu record_id=%llu",
        (unsigned long long)
            s_status.next_offset,
        (unsigned long long)
            s_status.last_acked_record_id
    );

    return ESP_OK;
}

esp_err_t upload_cursor_get(
    upload_cursor_status_t *output
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

esp_err_t upload_cursor_commit(
    uint64_t acknowledged_record_id,
    uint64_t next_offset
)
{
    if (!s_status.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if ((acknowledged_record_id == 0U) ||
        (acknowledged_record_id <=
         s_status.last_acked_record_id) ||
        (next_offset <
         s_status.next_offset)) {

        return ESP_ERR_INVALID_ARG;
    }

    upload_cursor_blob_t blob;

    memset(&blob, 0, sizeof(blob));

    blob.magic =
        UPLOAD_CURSOR_MAGIC;

    blob.version =
        UPLOAD_CURSOR_VERSION;

    blob.size =
        (uint16_t)sizeof(blob);

    blob.next_offset =
        next_offset;

    blob.last_acked_record_id =
        acknowledged_record_id;

    blob.crc =
        upload_cursor_blob_crc(
            &blob
        );

    esp_err_t result =
        upload_cursor_write_blob(
            &blob
        );

    if (result != ESP_OK) {
        ESP_LOGE(
            TAG,
            "Cursor commit failed: %s",
            esp_err_to_name(result)
        );

        return result;
    }

    /*
     * RAM state is updated only after NVS commit succeeds.
     */
    s_status.next_offset =
        next_offset;

    s_status.last_acked_record_id =
        acknowledged_record_id;

    ESP_LOGI(
        TAG,
        "Cursor committed: offset=%llu record_id=%llu",
        (unsigned long long)next_offset,
        (unsigned long long)
            acknowledged_record_id
    );

    return ESP_OK;
}

esp_err_t upload_cursor_reset(void)
{
    if (!s_status.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    upload_cursor_blob_t blob;

    memset(&blob, 0, sizeof(blob));

    blob.magic =
        UPLOAD_CURSOR_MAGIC;

    blob.version =
        UPLOAD_CURSOR_VERSION;

    blob.size =
        (uint16_t)sizeof(blob);

    blob.next_offset = 0U;
    blob.last_acked_record_id = 0U;

    blob.crc =
        upload_cursor_blob_crc(
            &blob
        );

    esp_err_t result =
        upload_cursor_write_blob(
            &blob
        );

    if (result != ESP_OK) {
        ESP_LOGE(
            TAG,
            "Cursor reset failed: %s",
            esp_err_to_name(result)
        );

        return result;
    }

    s_status.next_offset = 0U;
    s_status.last_acked_record_id = 0U;

    ESP_LOGW(
        TAG,
        "Upload cursor reset to offset 0"
    );

    return ESP_OK;
}

bool upload_cursor_is_initialized(void)
{
    return s_status.initialized;
}