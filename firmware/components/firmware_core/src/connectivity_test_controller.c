#include "connectivity_test_controller.h"

#include <stdbool.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "wifi_manager.h"

static const char *TAG = "CONNECTIVITY_TEST";

#define CONNECTIVITY_TEST_TASK_NAME       "connectivity_test"
#define CONNECTIVITY_TEST_TASK_STACK      3072U
#define CONNECTIVITY_TEST_TASK_PRIORITY   4U

#define CONNECTIVITY_TEST_ONLINE_MS       30000U
#define CONNECTIVITY_TEST_OFFLINE_MS      60000U

static TaskHandle_t s_task_handle = NULL;
static bool s_started = false;

static void connectivity_test_task(void *argument)
{
    (void)argument;

    ESP_LOGW(
        TAG,
        "E2E connectivity test armed: online for %u ms",
        (unsigned)CONNECTIVITY_TEST_ONLINE_MS
    );

    vTaskDelay(
        pdMS_TO_TICKS(
            CONNECTIVITY_TEST_ONLINE_MS
        )
    );

    ESP_LOGW(
        TAG,
        "E2E TEST PHASE: FORCED OFFLINE for %u ms",
        (unsigned)CONNECTIVITY_TEST_OFFLINE_MS
    );

    esp_err_t result =
        wifi_manager_test_force_offline();

    if (result != ESP_OK) {
        ESP_LOGE(
            TAG,
            "Failed to force Wi-Fi offline: %s",
            esp_err_to_name(result)
        );

        s_task_handle = NULL;
        vTaskDelete(NULL);
        return;
    }

    vTaskDelay(
        pdMS_TO_TICKS(
            CONNECTIVITY_TEST_OFFLINE_MS
        )
    );

    ESP_LOGW(
        TAG,
        "E2E TEST PHASE: RESTORE ONLINE"
    );

    result =
        wifi_manager_test_restore_online();

    if (result != ESP_OK) {
        ESP_LOGE(
            TAG,
            "Failed to restore Wi-Fi: %s",
            esp_err_to_name(result)
        );
    } else {
        ESP_LOGW(
            TAG,
            "Wi-Fi reconnect requested; normal recovery is active again"
        );
    }

    /*
     * One-shot test. Do not disconnect again.
     */
    s_task_handle = NULL;
    vTaskDelete(NULL);
}

esp_err_t connectivity_test_controller_start(void)
{
    if (s_started ||
        (s_task_handle != NULL)) {

        return ESP_ERR_INVALID_STATE;
    }

    s_started = true;

    BaseType_t result =
        xTaskCreate(
            connectivity_test_task,
            CONNECTIVITY_TEST_TASK_NAME,
            CONNECTIVITY_TEST_TASK_STACK,
            NULL,
            CONNECTIVITY_TEST_TASK_PRIORITY,
            &s_task_handle
        );

    if (result != pdPASS) {
        s_task_handle = NULL;
        s_started = false;
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}
