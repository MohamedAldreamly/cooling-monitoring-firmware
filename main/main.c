#include "alarm_engine.h"
#include "app_queues.h"
#include "esp_err.h"
#include "esp_log.h"
#include "record_builder.h"
#include "pipeline_test_monitor.h"
#include "signal_processing.h"
#include "signal_registry.h"
#include "signal_snapshot_manager.h"
#include "simulation_engine.h"
#include "storage_manager.h"
#include "system_identity.h"
#include "upload_cursor.h"
#include "cloud_uploader.h"

static const char *TAG = "MAIN";

void app_main(void)
{
    ESP_LOGI(
        TAG,
        "Starting IoT Cooling Monitoring Firmware"
    );

    ESP_ERROR_CHECK(
        signal_registry_init()
    );

    ESP_ERROR_CHECK(
        app_queues_init()
    );

    /*
     * Persistent IDs are initialized before Storage
     * and Record Builder.
     */
    ESP_ERROR_CHECK(
        system_identity_init()
    );

    /*
     * Journal recovery is completed before producers start.
     */
    ESP_ERROR_CHECK(
        storage_manager_init()
    );

    ESP_ERROR_CHECK(
        upload_cursor_init()
    );

    storage_manager_status_t storage_status;
upload_cursor_status_t cursor_status;

ESP_ERROR_CHECK(
    storage_manager_get_status(
        &storage_status
    )
);

ESP_ERROR_CHECK(
    upload_cursor_get(
        &cursor_status
    )
);

if (cursor_status.next_offset >
    storage_status.journal_size_bytes) {

    ESP_LOGW(
        TAG,
        "Upload cursor exceeds journal: cursor=%llu journal=%llu",
        (unsigned long long)
            cursor_status.next_offset,
        (unsigned long long)
            storage_status.journal_size_bytes
    );

    ESP_ERROR_CHECK(
        upload_cursor_reset()
    );
}

    ESP_ERROR_CHECK(
        record_builder_init()
    );

    ESP_ERROR_CHECK(
        cloud_uploader_init()
    );

    ESP_ERROR_CHECK(
        alarm_engine_init()
    );

    ESP_ERROR_CHECK(
        signal_snapshot_manager_init()
    );

    ESP_ERROR_CHECK(
        signal_processing_init()
    );

    /*
     * Start consumers from the end of the pipeline.
     */
    ESP_ERROR_CHECK(
        storage_manager_start()
    );

    ESP_ERROR_CHECK(
        cloud_uploader_start()
    );

    ESP_ERROR_CHECK(
        record_builder_start()
    );

    ESP_ERROR_CHECK(
        alarm_engine_start()
    );

    ESP_ERROR_CHECK(
        signal_snapshot_manager_start()
    );

    ESP_ERROR_CHECK(
        signal_processing_start()
    );

#if CONFIG_FIRMWARE_E2E_STORAGE_TEST
    ESP_ERROR_CHECK(
        pipeline_test_monitor_start()
    );

    ESP_ERROR_CHECK(
        simulation_engine_init(
            SIMULATION_SCENARIO_STORAGE_PIPELINE_TEST
        )
    );
#else
    ESP_ERROR_CHECK(
        simulation_engine_init(
            SIMULATION_SCENARIO_FULL_SYSTEM
        )
    );
#endif

    ESP_ERROR_CHECK(
        simulation_engine_start()
    );

    ESP_LOGI(
        TAG,
        "Simulation → Processing → Snapshot → Alarm → Record → Storage pipeline started"
    );
}
