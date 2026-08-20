idf_component_register(
    SRCS
        "src/app_types.c"
        "src/signal_registry.c"
        "src/app_queues.c"
        "src/simulation_engine.c"
        "src/signal_processing.c"
        "src/signal_snapshot_manager.c"
        "src/alarm_engine.c"
        "src/record_builder.c"
        "src/storage_manager.c"
        "src/system_identity.c"
        "src/cloud_payload.c"
        "src/pipeline_test_monitor.c"
        "src/upload_cursor.c"
        "src/cloud_uploader.c"
        "src/wifi_manager.c"
        "src/cloud_transport.c"
        "src/connectivity_test_controller.c"
        "src/runtime_shadow.c"
        "src/record_decoder.c"

    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "private_include"
    PRIV_REQUIRES
        esp_timer
        freertos
        log
        spiffs
        nvs_flash
        json
        esp_event
        esp_netif
        esp_wifi
        mqtt
        esp-tls
        mbedtls

    EMBED_TXTFILES
        "certs/device_certificate.pem.crt"
        "certs/device_private_key.pem.key"
)

