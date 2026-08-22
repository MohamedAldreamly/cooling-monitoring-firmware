#include "system_identity.h"

#include <string.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "system_identity_internal.h"

static const char *TAG = "SYSTEM_IDENTITY";

static system_identity_status_t s_status;

static nvs_handle_t s_nvs_handle = 0;

static SemaphoreHandle_t s_identity_mutex = NULL;

static uint64_t s_next_record_id = 0U;
static uint64_t s_record_id_limit = 0U;

static esp_err_t system_identity_initialize_nvs(void)
{
    esp_err_t result = nvs_flash_init();

    if ((result == ESP_ERR_NVS_NO_FREE_PAGES) ||
        (result == ESP_ERR_NVS_NEW_VERSION_FOUND)) {

        ESP_LOGW(
            TAG,
            "NVS requires erase: %s",
            esp_err_to_name(result)
        );

        result = nvs_flash_erase();

        if (result != ESP_OK) {
            ESP_LOGE(
                TAG,
                "NVS erase failed: %s",
                esp_err_to_name(result)
            );

            return result;
        }

        result = nvs_flash_init();
    }

    if (result != ESP_OK) {
        ESP_LOGE(
            TAG,
            "NVS initialization failed: %s",
            esp_err_to_name(result)
        );

        return result;
    }

    return ESP_OK;
}

static esp_err_t system_identity_open_namespace(void)
{
    esp_err_t result = nvs_open(
        SYSTEM_IDENTITY_NVS_NAMESPACE,
        NVS_READWRITE,
        &s_nvs_handle
    );

    if (result != ESP_OK) {
        ESP_LOGE(
            TAG,
            "Failed opening NVS namespace: %s",
            esp_err_to_name(result)
        );
    }

    return result;
}

static esp_err_t system_identity_increment_boot_id(void)
{
    uint32_t stored_boot_id = 0U;

    esp_err_t result = nvs_get_u32(
        s_nvs_handle,
        SYSTEM_IDENTITY_KEY_BOOT_ID,
        &stored_boot_id
    );

    if (result == ESP_ERR_NVS_NOT_FOUND) {
        stored_boot_id = 0U;
    } else if (result != ESP_OK) {
        ESP_LOGE(
            TAG,
            "Failed reading boot_id: %s",
            esp_err_to_name(result)
        );

        return result;
    }

    /*
     * Avoid wrapping to zero because zero means invalid/uninitialized.
     */
    if (stored_boot_id == UINT32_MAX) {
        ESP_LOGE(
            TAG,
            "boot_id counter exhausted"
        );

        return ESP_ERR_INVALID_STATE;
    }

    const uint32_t new_boot_id =
        stored_boot_id + 1U;

    result = nvs_set_u32(
        s_nvs_handle,
        SYSTEM_IDENTITY_KEY_BOOT_ID,
        new_boot_id
    );

    if (result != ESP_OK) {
        ESP_LOGE(
            TAG,
            "Failed writing boot_id: %s",
            esp_err_to_name(result)
        );

        return result;
    }

    result = nvs_commit(s_nvs_handle);

    if (result != ESP_OK) {
        ESP_LOGE(
            TAG,
            "Failed committing boot_id: %s",
            esp_err_to_name(result)
        );

        s_status.nvs_write_failures++;

        return result;
    }

    s_status.boot_id = new_boot_id;

    return ESP_OK;
}

static esp_err_t system_identity_load_record_limit(void)
{
    uint64_t stored_limit = 0U;

    esp_err_t result = nvs_get_u64(
        s_nvs_handle,
        SYSTEM_IDENTITY_KEY_RECORD_LIMIT,
        &stored_limit
    );

    if (result == ESP_ERR_NVS_NOT_FOUND) {
        stored_limit = 0U;
    } else if (result != ESP_OK) {
        ESP_LOGE(
            TAG,
            "Failed reading record limit: %s",
            esp_err_to_name(result)
        );

        return result;
    }

    /*
     * Never reuse a previously reserved ID range.
     *
     * If NVS stored 256, then IDs up to 256 may have been used
     * during an earlier boot. The next ID starts at 257.
     */
    s_next_record_id = stored_limit + 1U;
    s_record_id_limit = stored_limit;

    s_status.last_issued_record_id =
        stored_limit;

    s_status.reserved_record_id_limit =
        stored_limit;

    return ESP_OK;
}

static esp_err_t system_identity_reserve_record_block(void)
{
    if (s_record_id_limit >
        (UINT64_MAX -
         SYSTEM_IDENTITY_RECORD_BLOCK_SIZE)) {

        ESP_LOGE(
            TAG,
            "record_id counter exhausted"
        );

        return ESP_ERR_INVALID_STATE;
    }

    uint64_t base_limit = s_record_id_limit;

    /*
     * On first boot:
     * next_record_id = 1
     * current limit  = 0
     * new limit      = 256
     */
    const uint64_t new_limit =
        base_limit +
        SYSTEM_IDENTITY_RECORD_BLOCK_SIZE;

    esp_err_t result = nvs_set_u64(
        s_nvs_handle,
        SYSTEM_IDENTITY_KEY_RECORD_LIMIT,
        new_limit
    );

    if (result != ESP_OK) {
        ESP_LOGE(
            TAG,
            "Failed writing record limit: %s",
            esp_err_to_name(result)
        );

        s_status.nvs_write_failures++;

        return result;
    }

    result = nvs_commit(s_nvs_handle);

    if (result != ESP_OK) {
        ESP_LOGE(
            TAG,
            "Failed committing record limit: %s",
            esp_err_to_name(result)
        );

        s_status.nvs_write_failures++;

        return result;
    }

    /*
     * If there was no active block, start immediately after
     * the previously committed high-water mark.
     */
    if (s_next_record_id <= base_limit) {
        s_next_record_id = base_limit + 1U;
    }

    s_record_id_limit = new_limit;

    s_status.reserved_record_id_limit =
        new_limit;

    s_status.reserved_blocks++;

    ESP_LOGI(
        TAG,
        "Reserved record IDs: first=%llu last=%llu",
        (unsigned long long)s_next_record_id,
        (unsigned long long)s_record_id_limit
    );

    return ESP_OK;
}

esp_err_t system_identity_init(void)
{
    if (s_status.initialized) {
        return ESP_OK;
    }

    memset(&s_status, 0, sizeof(s_status));

    if (s_identity_mutex == NULL) {
        s_identity_mutex = xSemaphoreCreateMutex();

        if (s_identity_mutex == NULL) {
            return ESP_ERR_NO_MEM;
        }
    }

    esp_err_t result =
        system_identity_initialize_nvs();

    if (result != ESP_OK) {
        s_status.degraded = true;

        return result;
    }

    result = system_identity_open_namespace();

    if (result != ESP_OK) {
        s_status.degraded = true;

        return result;
    }

    result = system_identity_increment_boot_id();

    if (result != ESP_OK) {
        nvs_close(s_nvs_handle);
        s_nvs_handle = 0;

        s_status.degraded = true;

        return result;
    }

    result = system_identity_load_record_limit();

    if (result != ESP_OK) {
        nvs_close(s_nvs_handle);
        s_nvs_handle = 0;

        s_status.degraded = true;

        return result;
    }

    /*
     * Reserve the first block for this boot before any producer starts.
     */
    result = system_identity_reserve_record_block();

    if (result != ESP_OK) {
        nvs_close(s_nvs_handle);
        s_nvs_handle = 0;

        s_status.degraded = true;

        return result;
    }

    s_status.initialized = true;
    s_status.degraded = false;

    ESP_LOGI(
        TAG,
        "Identity initialized: boot_id=%lu next_record_id=%llu reserved_limit=%llu",
        (unsigned long)s_status.boot_id,
        (unsigned long long)s_next_record_id,
        (unsigned long long)s_record_id_limit
    );

    return ESP_OK;
}

esp_err_t system_identity_get_boot_id(
    uint32_t *boot_id
)
{
    if (boot_id == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!s_status.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    *boot_id = s_status.boot_id;

    return ESP_OK;
}

esp_err_t system_identity_next_record_id(
    uint64_t *record_id
)
{
    if (record_id == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!s_status.initialized ||
        s_identity_mutex == NULL) {

        return ESP_ERR_INVALID_STATE;
    }

    if (xSemaphoreTake(
            s_identity_mutex,
            pdMS_TO_TICKS(100)
        ) != pdTRUE) {

        return ESP_ERR_TIMEOUT;
    }

    esp_err_t result = ESP_OK;

    if (s_next_record_id >
        s_record_id_limit) {

        result =
            system_identity_reserve_record_block();
    }

    if (result == ESP_OK) {
        *record_id = s_next_record_id;

        s_status.last_issued_record_id =
            s_next_record_id;

        s_next_record_id++;
    }

    xSemaphoreGive(s_identity_mutex);

    return result;
}

esp_err_t system_identity_get_status(
    system_identity_status_t *output
)
{
    if (output == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!s_status.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (xSemaphoreTake(
            s_identity_mutex,
            pdMS_TO_TICKS(50)
        ) != pdTRUE) {

        return ESP_ERR_TIMEOUT;
    }

    *output = s_status;

    xSemaphoreGive(s_identity_mutex);

    return ESP_OK;
}

bool system_identity_is_initialized(void)
{
    return s_status.initialized;
}