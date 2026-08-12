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

#include "cloud_contract.h"
#include "cloud_payload.h"

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
    
    uint32_t boot_id = 0U;

	ESP_ERROR_CHECK(
    	system_identity_get_boot_id(
        	&boot_id
    	)
	);

	static char status_payload[
	    	CLOUD_MQTT_PAYLOAD_MAX_LEN
	];

	ESP_ERROR_CHECK(
    	cloud_payload_encode_status(
        	boot_id,
        	0U,
        	true,
        	"online",
        	status_payload,
        	sizeof(status_payload)
    	)
	);

	ESP_LOGI(
    	TAG,
    	"MQTT PREVIEW TOPIC: %s",
    	CLOUD_TOPIC_STATUS
	);

	ESP_LOGI(
    	TAG,
    	"MQTT PREVIEW PAYLOAD: %s",
    	status_payload
	);

    /*
     * Journal recovery is completed before producers start.
     */
    ESP_ERROR_CHECK(
        storage_manager_init()
    );

    ESP_ERROR_CHECK(
        record_builder_init()
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
