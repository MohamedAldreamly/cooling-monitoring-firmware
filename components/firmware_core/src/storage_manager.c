#include "storage_manager.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "app_queues.h"
#include "esp_log.h"
#include "esp_spiffs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "storage_manager_internal.h"

static const char *TAG = "STORAGE_MANAGER";

#define STORAGE_WRITER_TASK_NAME              "storage_writer"
#define STORAGE_WRITER_TASK_STACK_SIZE        6144U
#define STORAGE_WRITER_TASK_PRIORITY          6U

#define STORAGE_RECORD_RECEIVE_TIMEOUT_MS     1000U

#define STORAGE_COPY_BUFFER_SIZE              256U

static TaskHandle_t s_task_handle = NULL;

static storage_manager_status_t s_status;

static bool s_stop_requested = false;

/*
 * Standard CRC-32:
 * Polynomial 0xEDB88320.
 *
 * Implemented locally to keep the journal format independent
 * from a specific hardware CRC peripheral.
 */
static uint32_t storage_crc32(
    const void *data,
    size_t length
)
{
    if ((data == NULL) && (length > 0U)) {
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
                (uint32_t)(-(int32_t)(crc & 1U));

            crc =
                (crc >> 1U) ^
                (0xEDB88320UL & mask);
        }
    }

    return ~crc;
}

static uint32_t storage_header_crc(
    const storage_record_header_t *header
)
{
    if (header == NULL) {
        return 0U;
    }

    storage_record_header_t copy = *header;

    copy.header_crc = 0U;

    return storage_crc32(
        &copy,
        sizeof(copy)
    );
}

static bool storage_header_is_valid(
    const storage_record_header_t *header
)
{
    if (header == NULL) {
        return false;
    }

    if (header->magic != STORAGE_RECORD_MAGIC) {
        return false;
    }

    if (header->format_version !=
        STORAGE_RECORD_FORMAT_VERSION) {

        return false;
    }

    if (header->header_size !=
        sizeof(storage_record_header_t)) {

        return false;
    }

    if (header->payload_length >
        APP_RECORD_PAYLOAD_MAX_LEN) {

        return false;
    }

    const uint32_t expected_crc =
        storage_header_crc(header);

    return expected_crc == header->header_crc;
}

static esp_err_t storage_flush_file(FILE *file)
{
    if (file == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (fflush(file) != 0) {
        ESP_LOGE(
            TAG,
            "fflush failed: errno=%d",
            errno
        );

        return ESP_FAIL;
    }

    return ESP_OK;
}

static esp_err_t storage_mount(void)
{
    esp_vfs_spiffs_conf_t configuration = {
        .base_path = STORAGE_MOUNT_PATH,
        .partition_label =
            STORAGE_PARTITION_LABEL,
        .max_files = 5,
        .format_if_mount_failed = true,
    };

    esp_err_t result =
        esp_vfs_spiffs_register(
            &configuration
        );

    if (result != ESP_OK) {
        ESP_LOGE(
            TAG,
            "SPIFFS mount failed: %s",
            esp_err_to_name(result)
        );

        return result;
    }

    size_t total_bytes = 0U;
    size_t used_bytes = 0U;

    result = esp_spiffs_info(
        STORAGE_PARTITION_LABEL,
        &total_bytes,
        &used_bytes
    );

    if (result != ESP_OK) {
        ESP_LOGW(
            TAG,
            "Failed reading SPIFFS info: %s",
            esp_err_to_name(result)
        );
    } else {
        ESP_LOGI(
            TAG,
            "SPIFFS mounted: total=%u used=%u",
            (unsigned int)total_bytes,
            (unsigned int)used_bytes
        );
    }

    s_status.mounted = true;

    return ESP_OK;
}

static uint64_t storage_file_size(
    const char *path
)
{
    struct stat file_stat;

    if ((path == NULL) ||
        (stat(path, &file_stat) != 0)) {

        return 0U;
    }

    return (uint64_t)file_stat.st_size;
}

static esp_err_t storage_build_header(
    const durable_record_t *record,
    storage_record_header_t *header
)
{
    if ((record == NULL) ||
        (header == NULL)) {

        return ESP_ERR_INVALID_ARG;
    }

    if (record->payload_length >
        APP_RECORD_PAYLOAD_MAX_LEN) {

        return ESP_ERR_INVALID_SIZE;
    }

    memset(header, 0, sizeof(*header));

    header->magic =
        STORAGE_RECORD_MAGIC;

    header->format_version =
        STORAGE_RECORD_FORMAT_VERSION;

    header->header_size =
        sizeof(storage_record_header_t);

    header->record_type =
        (uint16_t)record->type;

    header->record_priority =
        (uint16_t)record->priority;

    header->record_id =
        record->record_id;

    header->boot_id =
        record->boot_id;

    header->boot_sequence =
        record->boot_sequence;

    header->uptime_ms =
        record->uptime_ms;

    header->observed_at_ms =
        record->observed_at_ms;

    header->payload_length =
        record->payload_length;

    header->requires_application_ack =
        record->requires_application_ack
            ? 1U
            : 0U;

    header->header_crc =
        storage_header_crc(header);

    return ESP_OK;
}

esp_err_t storage_manager_append(
    const durable_record_t *record
)
{
    if (record == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!s_status.initialized ||
        !s_status.mounted) {

        return ESP_ERR_INVALID_STATE;
    }

    storage_record_header_t header;

    esp_err_t result =
        storage_build_header(
            record,
            &header
        );

    if (result != ESP_OK) {
        return result;
    }

    storage_record_footer_t footer = {
        .payload_crc = storage_crc32(
            record->payload,
            record->payload_length
        ),
        .commit_marker =
            STORAGE_RECORD_COMMIT_MARKER,
    };

    FILE *file = fopen(
        STORAGE_JOURNAL_PATH,
        "ab"
    );

    if (file == NULL) {
        ESP_LOGE(
            TAG,
            "Failed opening journal: errno=%d",
            errno
        );

        s_status.degraded = true;

        return ESP_FAIL;
    }

    bool write_ok = true;

    if (fwrite(
            &header,
            sizeof(header),
            1U,
            file
        ) != 1U) {

        write_ok = false;
    }

    if (write_ok &&
        record->payload_length > 0U) {

        if (fwrite(
                record->payload,
                record->payload_length,
                1U,
                file
            ) != 1U) {

            write_ok = false;
        }
    }

    /*
     * The commit footer is written last.
     * A record without this footer is incomplete.
     */
    if (write_ok) {
        if (fwrite(
                &footer,
                sizeof(footer),
                1U,
                file
            ) != 1U) {

            write_ok = false;
        }
    }

    if (!write_ok) {
        ESP_LOGE(
            TAG,
            "Journal write failed: record_id=%llu errno=%d",
            (unsigned long long)record->record_id,
            errno
        );

        fclose(file);

        s_status.degraded = true;

        return ESP_FAIL;
    }

    result = storage_flush_file(file);

    if (fclose(file) != 0) {
        ESP_LOGW(
            TAG,
            "Journal close failed: errno=%d",
            errno
        );

        if (result == ESP_OK) {
            result = ESP_FAIL;
        }
    }

    if (result != ESP_OK) {
        s_status.flush_failures++;
        s_status.degraded = true;

        return result;
    }

    s_status.last_written_record_id =
        record->record_id;

    s_status.journal_size_bytes =
        storage_file_size(
            STORAGE_JOURNAL_PATH
        );

    return ESP_OK;
}

static esp_err_t storage_copy_valid_prefix(
    uint64_t valid_bytes
)
{
    FILE *source = fopen(
        STORAGE_JOURNAL_PATH,
        "rb"
    );

    if (source == NULL) {
        return ESP_FAIL;
    }

    FILE *target = fopen(
        STORAGE_REPAIR_PATH,
        "wb"
    );

    if (target == NULL) {
        fclose(source);

        return ESP_FAIL;
    }

    uint8_t buffer[STORAGE_COPY_BUFFER_SIZE];

    uint64_t remaining = valid_bytes;

    esp_err_t result = ESP_OK;

    while (remaining > 0U) {
        const size_t requested =
            remaining >
                STORAGE_COPY_BUFFER_SIZE
                ? STORAGE_COPY_BUFFER_SIZE
                : (size_t)remaining;

        const size_t bytes_read =
            fread(
                buffer,
                1U,
                requested,
                source
            );

        if (bytes_read != requested) {
            result = ESP_FAIL;
            break;
        }

        const size_t bytes_written =
            fwrite(
                buffer,
                1U,
                bytes_read,
                target
            );

        if (bytes_written != bytes_read) {
            result = ESP_FAIL;
            break;
        }

        remaining -= bytes_written;
    }

    if ((result == ESP_OK) &&
        (storage_flush_file(target) !=
         ESP_OK)) {

        result = ESP_FAIL;
    }

    fclose(source);
    fclose(target);

    if (result != ESP_OK) {
        remove(STORAGE_REPAIR_PATH);

        return result;
    }

    if (remove(STORAGE_JOURNAL_PATH) != 0) {
        remove(STORAGE_REPAIR_PATH);

        return ESP_FAIL;
    }

    if (rename(
            STORAGE_REPAIR_PATH,
            STORAGE_JOURNAL_PATH
        ) != 0) {

        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t storage_manager_recover(
    storage_recovery_report_t *report
)
{
    if (report == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!s_status.mounted) {
        return ESP_ERR_INVALID_STATE;
    }

    memset(report, 0, sizeof(*report));

    report->original_bytes =
        storage_file_size(
            STORAGE_JOURNAL_PATH
        );

    FILE *file = fopen(
        STORAGE_JOURNAL_PATH,
        "rb"
    );

    if (file == NULL) {
        /*
         * No journal yet is a normal first-boot state.
         */
        if (errno == ENOENT) {
            return ESP_OK;
        }

        return ESP_FAIL;
    }

    uint64_t valid_bytes = 0U;

    while (true) {
        storage_record_header_t header;

        const size_t header_read =
            fread(
                &header,
                1U,
                sizeof(header),
                file
            );

        if (header_read == 0U) {
            if (feof(file)) {
                break;
            }

            fclose(file);

            return ESP_FAIL;
        }

        if (header_read != sizeof(header)) {
            report->incomplete_records++;
            report->repair_required = true;

            break;
        }

        if (!storage_header_is_valid(
                &header
            )) {

            report->corrupted_records++;
            report->repair_required = true;

            break;
        }

        uint8_t payload[
            APP_RECORD_PAYLOAD_MAX_LEN
        ];

        memset(payload, 0, sizeof(payload));

        if (header.payload_length > 0U) {
            const size_t payload_read =
                fread(
                    payload,
                    1U,
                    header.payload_length,
                    file
                );

            if (payload_read !=
                header.payload_length) {

                report->incomplete_records++;
                report->repair_required = true;

                break;
            }
        }

        storage_record_footer_t footer;

        const size_t footer_read =
            fread(
                &footer,
                1U,
                sizeof(footer),
                file
            );

        if (footer_read != sizeof(footer)) {
            report->incomplete_records++;
            report->repair_required = true;

            break;
        }

        if (footer.commit_marker !=
            STORAGE_RECORD_COMMIT_MARKER) {

            report->incomplete_records++;
            report->repair_required = true;

            break;
        }

        const uint32_t payload_crc =
            storage_crc32(
                payload,
                header.payload_length
            );

        if (payload_crc !=
            footer.payload_crc) {

            report->corrupted_records++;
            report->repair_required = true;

            break;
        }

        valid_bytes +=
            sizeof(header) +
            header.payload_length +
            sizeof(footer);

        report->valid_records++;

        report->last_valid_record_id =
            header.record_id;
    }

    fclose(file);

    report->valid_bytes = valid_bytes;

    if (report->repair_required) {
        esp_err_t repair_result =
            storage_copy_valid_prefix(
                valid_bytes
            );

        if (repair_result != ESP_OK) {
            ESP_LOGE(
                TAG,
                "Journal repair failed"
            );

            return repair_result;
        }

        report->repair_completed = true;

        ESP_LOGW(
            TAG,
            "Journal repaired: valid_bytes=%llu original_bytes=%llu",
            (unsigned long long)
                report->valid_bytes,
            (unsigned long long)
                report->original_bytes
        );
    }

    s_status.recovered_records =
        report->valid_records;

    s_status.corrupted_records =
        report->corrupted_records;

    s_status.incomplete_records =
        report->incomplete_records;

    if (report->repair_completed) {
        s_status.journal_repairs++;
    }

    s_status.last_written_record_id =
        report->last_valid_record_id;

    s_status.journal_size_bytes =
        storage_file_size(
            STORAGE_JOURNAL_PATH
        );

    return ESP_OK;
}

static void storage_writer_task(
    void *argument
)
{
    (void)argument;

    durable_record_t record;

    s_status.running = true;
    s_stop_requested = false;

    ESP_LOGI(
        TAG,
        "Storage Writer task started"
    );

    while (!s_stop_requested) {
        esp_err_t receive_result =
            app_queues_receive_record(
                &record,
                pdMS_TO_TICKS(
                    STORAGE_RECORD_RECEIVE_TIMEOUT_MS
                )
            );

        if (receive_result ==
            ESP_ERR_TIMEOUT) {

            continue;
        }

        if (receive_result != ESP_OK) {
            ESP_LOGW(
                TAG,
                "Record receive failed: %s",
                esp_err_to_name(
                    receive_result
                )
            );

            continue;
        }

        s_status.records_received++;

        esp_err_t append_result =
            storage_manager_append(
                &record
            );

        if (append_result != ESP_OK) {
            s_status.write_failures++;
            s_status.degraded = true;

            ESP_LOGE(
                TAG,
                "Record append failed: id=%llu error=%s",
                (unsigned long long)
                    record.record_id,
                esp_err_to_name(
                    append_result
                )
            );

            continue;
        }

        s_status.records_written++;

        ESP_LOGI(
            TAG,
            "Record committed: id=%llu type=%s bytes=%u journal=%llu",
            (unsigned long long)
                record.record_id,
            record_type_to_string(
                record.type
            ),
            (unsigned int)
                record.payload_length,
            (unsigned long long)
                s_status.journal_size_bytes
        );
    }

    s_status.running = false;
    s_task_handle = NULL;

    ESP_LOGI(
        TAG,
        "Storage Writer task stopped"
    );

    vTaskDelete(NULL);
}

esp_err_t storage_manager_init(void)
{
    if (s_status.running) {
        return ESP_ERR_INVALID_STATE;
    }

    memset(&s_status, 0, sizeof(s_status));

    esp_err_t mount_result =
        storage_mount();

    if (mount_result != ESP_OK) {
        s_status.degraded = true;

        return mount_result;
    }

    storage_recovery_report_t report;

    esp_err_t recovery_result =
        storage_manager_recover(
            &report
        );

    if (recovery_result != ESP_OK) {
        s_status.degraded = true;

        return recovery_result;
    }

    s_status.initialized = true;
    s_stop_requested = false;

    ESP_LOGI(
        TAG,
        "Storage initialized: records=%lu size=%llu repaired=%s",
        (unsigned long)
            report.valid_records,
        (unsigned long long)
            s_status.journal_size_bytes,
        report.repair_completed
            ? "true"
            : "false"
    );

    return ESP_OK;
}

esp_err_t storage_manager_start(void)
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
        storage_writer_task,
        STORAGE_WRITER_TASK_NAME,
        STORAGE_WRITER_TASK_STACK_SIZE,
        NULL,
        STORAGE_WRITER_TASK_PRIORITY,
        &s_task_handle
    );

    if (result != pdPASS) {
        s_task_handle = NULL;

        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}

esp_err_t storage_manager_stop(void)
{
    if (!s_status.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    s_stop_requested = true;

    return ESP_OK;
}

esp_err_t storage_manager_get_status(
    storage_manager_status_t *output
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

bool storage_manager_is_running(void)
{
    return s_status.running;
}
