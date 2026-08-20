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
#include "cloud_transport.h"
#include "connectivity_test_controller.h"
#include "wifi_manager.h"
#include "runtime_shadow.h"

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

/*
 * Wi-Fi initialization must always run when enabled.
 * It is unrelated to the cursor validation condition.
 */
#if CONFIG_FIRMWARE_WIFI_ENABLED
    ESP_ERROR_CHECK(
        wifi_manager_init()
    );

    cloud_transport_config_t cloud_transport_config = {
        .event_callback =
            cloud_uploader_transport_event_callback,

        .user_context =
            NULL,

        .server_root_ca_pem =
            NULL,
    };

    ESP_ERROR_CHECK(
        cloud_transport_init(
            &cloud_transport_config
        )
    );

    ESP_ERROR_CHECK(
        runtime_shadow_init()
    );
#endif

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

#if CONFIG_FIRMWARE_WIFI_ENABLED
    ESP_ERROR_CHECK(
        wifi_manager_start()
    );

    esp_err_t wifi_result =
        wifi_manager_wait_connected(
            30000U
        );

    if (wifi_result != ESP_OK) {
        ESP_LOGW(
            TAG,
            "Wi-Fi not connected yet: %s",
            esp_err_to_name(wifi_result)
        );

        /*
         * Do not start AWS transport until Wi-Fi is available.
         * The rest of the local pipeline can still continue.
         */
    } else {
        ESP_LOGI(
            TAG,
            "Wi-Fi connected; starting AWS IoT transport"
        );

        ESP_ERROR_CHECK(
            cloud_transport_start()
        );
    }
#endif

    /*
     * cloud_uploader remains enabled, but cursor advancement must only
     * happen after a validated Application ACK from AWS, not after the
     * broker-level MQTT PUBACK.
     */
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

#if CONFIG_FIRMWARE_WIFI_ENABLED && CONFIG_FIRMWARE_E2E_STORAGE_TEST
    /*
     * One-shot connectivity fault injection for the Wokwi E2E test:
     *
     *   30 s online
     *   60 s forced offline
     *   then Wi-Fi is restored and left online
     *
     * The simulation/storage pipeline continues running while offline, so
     * new alarm records accumulate in journal.bin. When connectivity returns,
     * cloud_transport reconnects and cloud_uploader drains the backlog from
     * the persisted upload cursor.
     */
    ESP_ERROR_CHECK(
        connectivity_test_controller_start()
    );
#endif

    ESP_LOGI(
        TAG,
        "Simulation → Processing → Snapshot → Alarm → Record → Storage pipeline started"
    );

#if CONFIG_FIRMWARE_WIFI_ENABLED
    ESP_LOGI(
        TAG,
        "Cloud path enabled: Wi-Fi → AWS IoT MQTT/TLS"
    );
#endif
}