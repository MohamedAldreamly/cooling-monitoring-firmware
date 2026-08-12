#include "pipeline_test_monitor.h"

#include <stdbool.h>
#include <string.h>

#include "alarm_engine.h"
#include "app_queues.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "record_builder.h"
#include "signal_processing.h"
#include "signal_snapshot_manager.h"
#include "simulation_engine.h"
#include "storage_manager.h"

static const char *TAG = "PIPELINE_E2E_TEST";

#define PIPELINE_TEST_TASK_NAME          "pipeline_test"
#define PIPELINE_TEST_TASK_STACK_SIZE    4096U
#define PIPELINE_TEST_TASK_PRIORITY      3U
#define PIPELINE_TEST_DURATION_MS        30000U
#define PIPELINE_EXPECTED_TRANSITIONS    4U

typedef struct {
    app_queue_counters_t queues;
    signal_processing_status_t processing;
    signal_snapshot_manager_status_t snapshots;
    alarm_engine_status_t alarms;
    record_builder_status_t records;
    storage_manager_status_t storage;
} pipeline_test_baseline_t;

static pipeline_test_baseline_t s_baseline;
static TaskHandle_t s_task_handle = NULL;

static esp_err_t pipeline_test_capture_baseline(void)
{
    memset(&s_baseline, 0, sizeof(s_baseline));

    esp_err_t result = app_queues_get_counters(&s_baseline.queues);
    if (result != ESP_OK) {
        return result;
    }

    result = signal_processing_get_status(&s_baseline.processing);
    if (result != ESP_OK) {
        return result;
    }

    result = signal_snapshot_manager_get_status(&s_baseline.snapshots);
    if (result != ESP_OK) {
        return result;
    }

    result = alarm_engine_get_status(&s_baseline.alarms);
    if (result != ESP_OK) {
        return result;
    }

    result = record_builder_get_status(&s_baseline.records);
    if (result != ESP_OK) {
        return result;
    }

    return storage_manager_get_status(&s_baseline.storage);
}

static bool pipeline_test_no_new_drops(
    const app_queue_counters_t *queues,
    const simulation_engine_status_t *simulation,
    const signal_processing_status_t *processing,
    const signal_snapshot_manager_status_t *snapshots,
    const alarm_engine_status_t *alarms,
    const record_builder_status_t *records,
    const storage_manager_status_t *storage
)
{
    return queues->raw_sample_dropped ==
               s_baseline.queues.raw_sample_dropped &&
           queues->signal_value_dropped ==
               s_baseline.queues.signal_value_dropped &&
           queues->signal_snapshot_dropped ==
               s_baseline.queues.signal_snapshot_dropped &&
           queues->alarm_event_dropped ==
               s_baseline.queues.alarm_event_dropped &&
           queues->record_dropped ==
               s_baseline.queues.record_dropped &&
           simulation->queue_failures == 0U &&
           processing->queue_failures ==
               s_baseline.processing.queue_failures &&
           snapshots->snapshot_queue_failures ==
               s_baseline.snapshots.snapshot_queue_failures &&
           alarms->alarm_queue_failures ==
               s_baseline.alarms.alarm_queue_failures &&
           records->record_queue_failures ==
               s_baseline.records.record_queue_failures &&
           storage->write_failures ==
               s_baseline.storage.write_failures &&
           storage->flush_failures ==
               s_baseline.storage.flush_failures;
}

static void pipeline_test_task(void *argument)
{
    (void)argument;

    ESP_LOGI(
        TAG,
        "START: raw samples must traverse the complete production pipeline"
    );

    vTaskDelay(pdMS_TO_TICKS(PIPELINE_TEST_DURATION_MS));

    simulation_engine_status_t simulation;
    app_queue_counters_t queues;
    signal_processing_status_t processing;
    signal_snapshot_manager_status_t snapshots;
    alarm_engine_status_t alarms;
    record_builder_status_t records;
    storage_manager_status_t storage;

    const bool status_ok =
        simulation_engine_get_status(&simulation) == ESP_OK &&
        app_queues_get_counters(&queues) == ESP_OK &&
        signal_processing_get_status(&processing) == ESP_OK &&
        signal_snapshot_manager_get_status(&snapshots) == ESP_OK &&
        alarm_engine_get_status(&alarms) == ESP_OK &&
        record_builder_get_status(&records) == ESP_OK &&
        storage_manager_get_status(&storage) == ESP_OK;

    if (!status_ok) {
        ESP_LOGE(TAG, "FAIL: unable to read one or more component statuses");
        s_task_handle = NULL;
        vTaskDelete(NULL);
        return;
    }

    const uint32_t raw_delta =
        queues.raw_sample_sent - s_baseline.queues.raw_sample_sent;
    const uint32_t processed_delta =
        processing.samples_processed -
        s_baseline.processing.samples_processed;
    const uint32_t snapshot_delta =
        snapshots.snapshots_published -
        s_baseline.snapshots.snapshots_published;
    const uint32_t alarm_delta =
        alarms.alarm_events_generated -
        s_baseline.alarms.alarm_events_generated;
    const uint32_t record_delta =
        records.records_published -
        s_baseline.records.records_published;
    const uint32_t stored_delta =
        storage.records_written -
        s_baseline.storage.records_written;

    const bool raw_ok = raw_delta > 0U;
    const bool processing_ok = processed_delta > 0U;
    const bool snapshots_ok = snapshot_delta > 0U;
    const bool alarms_ok =
        alarm_delta >= PIPELINE_EXPECTED_TRANSITIONS &&
        alarms.active_alarms == 0U;
    const bool records_ok =
        record_delta >= PIPELINE_EXPECTED_TRANSITIONS;
    const bool storage_ok =
        stored_delta >= PIPELINE_EXPECTED_TRANSITIONS &&
        storage.journal_size_bytes >
            s_baseline.storage.journal_size_bytes &&
        !storage.degraded;
    const bool no_drops = pipeline_test_no_new_drops(
        &queues,
        &simulation,
        &processing,
        &snapshots,
        &alarms,
        &records,
        &storage
    );

    ESP_LOGI(TAG, "RAW -> queue:          %s (%lu samples)",
             raw_ok ? "PASS" : "FAIL", (unsigned long)raw_delta);
    ESP_LOGI(TAG, "Signal processing:     %s (%lu values)",
             processing_ok ? "PASS" : "FAIL",
             (unsigned long)processed_delta);
    ESP_LOGI(TAG, "Snapshot publishing:   %s (%lu snapshots)",
             snapshots_ok ? "PASS" : "FAIL",
             (unsigned long)snapshot_delta);
    ESP_LOGI(TAG, "Alarm lifecycle:       %s (%lu transitions)",
             alarms_ok ? "PASS" : "FAIL", (unsigned long)alarm_delta);
    ESP_LOGI(TAG, "Record Builder:        %s (%lu records)",
             records_ok ? "PASS" : "FAIL", (unsigned long)record_delta);
    ESP_LOGI(TAG, "SPIFFS Storage Writer: %s (%lu records, %llu bytes)",
             storage_ok ? "PASS" : "FAIL",
             (unsigned long)stored_delta,
             (unsigned long long)storage.journal_size_bytes);
    ESP_LOGI(TAG, "Queue/write drops:     %s", no_drops ? "PASS" : "FAIL");

    const bool passed =
        raw_ok && processing_ok && snapshots_ok && alarms_ok &&
        records_ok && storage_ok && no_drops;

    if (passed) {
        ESP_LOGI(
            TAG,
            "E2E TEST PASS: Raw -> Processing -> Snapshot -> Alarm -> Record -> SPIFFS"
        );
    } else {
        ESP_LOGE(TAG, "E2E TEST FAIL: inspect the failed stage above");
    }

    simulation_engine_stop();
    s_task_handle = NULL;
    vTaskDelete(NULL);
}

esp_err_t pipeline_test_monitor_start(void)
{
    if (s_task_handle != NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t result = pipeline_test_capture_baseline();
    if (result != ESP_OK) {
        return result;
    }

    const BaseType_t task_result = xTaskCreate(
        pipeline_test_task,
        PIPELINE_TEST_TASK_NAME,
        PIPELINE_TEST_TASK_STACK_SIZE,
        NULL,
        PIPELINE_TEST_TASK_PRIORITY,
        &s_task_handle
    );

    if (task_result != pdPASS) {
        s_task_handle = NULL;
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}
