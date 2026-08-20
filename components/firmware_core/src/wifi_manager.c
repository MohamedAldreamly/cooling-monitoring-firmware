#include "wifi_manager.h"

#include <string.h>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "sdkconfig.h"

static const char *TAG = "WIFI_MANAGER";

#define WIFI_CONNECTED_BIT    BIT0
#define WIFI_FAILED_BIT       BIT1

static EventGroupHandle_t s_event_group = NULL;

static esp_netif_t *s_station_netif = NULL;

static esp_event_handler_instance_t
    s_wifi_event_instance;

static esp_event_handler_instance_t
    s_ip_event_instance;

static wifi_manager_status_t s_status;

/*
 * E2E connectivity-test hook.
 *
 * When true, a deliberate esp_wifi_disconnect() must NOT trigger the normal
 * automatic reconnect path. The local firmware pipeline keeps running while
 * Wi-Fi/AWS are unavailable.
 */
static bool s_test_force_offline = false;

static void wifi_manager_event_handler(
    void *argument,
    esp_event_base_t event_base,
    int32_t event_id,
    void *event_data
)
{
    (void)argument;

    if ((event_base == WIFI_EVENT) &&
        (event_id == WIFI_EVENT_STA_START)) {

        s_status.connection_attempts++;

        esp_err_t result =
            esp_wifi_connect();

        if (result != ESP_OK) {
            ESP_LOGE(
                TAG,
                "Initial Wi-Fi connect failed: %s",
                esp_err_to_name(result)
            );
        }

        return;
    }

    if ((event_base == WIFI_EVENT) &&
        (event_id ==
         WIFI_EVENT_STA_DISCONNECTED)) {

        s_status.connected = false;
        s_status.has_ip = false;
        s_status.disconnections++;

        xEventGroupClearBits(
            s_event_group,
            WIFI_CONNECTED_BIT
        );

        /*
         * A deliberate E2E-test disconnect is different from a real network
         * failure. Keep the Wi-Fi driver running, but suppress automatic
         * reconnect until wifi_manager_test_restore_online() is called.
         */
        if (s_test_force_offline) {
            ESP_LOGW(
                TAG,
                "TEST: Wi-Fi intentionally offline; reconnect suppressed"
            );

            return;
        }

        if (s_status.retry_count <
            CONFIG_FIRMWARE_WIFI_MAXIMUM_RETRY) {

            s_status.retry_count++;
        } else {
            /*
             * Report failure to current waiters, but keep
             * reconnecting so cloud upload can recover
             * when the network returns.
             */
            xEventGroupSetBits(
                s_event_group,
                WIFI_FAILED_BIT
            );
        }

        s_status.connection_attempts++;

        esp_err_t result =
            esp_wifi_connect();

        if (result != ESP_OK) {
            ESP_LOGW(
                TAG,
                "Wi-Fi reconnect request failed: %s",
                esp_err_to_name(result)
            );
        } else {
            ESP_LOGW(
                TAG,
                "Wi-Fi disconnected; retry=%lu",
                (unsigned long)
                    s_status.retry_count
            );
        }

        return;
    }

    if ((event_base == IP_EVENT) &&
        (event_id == IP_EVENT_STA_GOT_IP)) {

        const ip_event_got_ip_t *ip_event =
            (const ip_event_got_ip_t *)
            event_data;

        s_status.connected = true;
        s_status.has_ip = true;
        s_status.retry_count = 0U;
        s_status.successful_connections++;

        xEventGroupClearBits(
            s_event_group,
            WIFI_FAILED_BIT
        );

        xEventGroupSetBits(
            s_event_group,
            WIFI_CONNECTED_BIT
        );

        ESP_LOGI(
            TAG,
            "Wi-Fi connected; IP=" IPSTR,
            IP2STR(&ip_event->ip_info.ip)
        );
    }
}

esp_err_t wifi_manager_init(void)
{
    if (s_status.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    memset(&s_status, 0, sizeof(s_status));
    s_test_force_offline = false;

    const size_t ssid_length =
        strlen(CONFIG_FIRMWARE_WIFI_SSID);

    const size_t password_length =
        strlen(CONFIG_FIRMWARE_WIFI_PASSWORD);

    if ((ssid_length == 0U) ||
        (ssid_length >
         sizeof(((wifi_config_t *)0)
                    ->sta.ssid)) ||
        (password_length >
         sizeof(((wifi_config_t *)0)
                    ->sta.password))) {

        return ESP_ERR_INVALID_ARG;
    }

    s_event_group =
        xEventGroupCreate();

    if (s_event_group == NULL) {
        return ESP_ERR_NO_MEM;
    }

    esp_err_t result =
        esp_netif_init();

    if ((result != ESP_OK) &&
        (result != ESP_ERR_INVALID_STATE)) {

        vEventGroupDelete(s_event_group);
        s_event_group = NULL;

        return result;
    }

    result =
        esp_event_loop_create_default();

    if ((result != ESP_OK) &&
        (result != ESP_ERR_INVALID_STATE)) {

        vEventGroupDelete(s_event_group);
        s_event_group = NULL;

        return result;
    }

    s_station_netif =
        esp_netif_create_default_wifi_sta();

    if (s_station_netif == NULL) {
        vEventGroupDelete(s_event_group);
        s_event_group = NULL;

        return ESP_ERR_NO_MEM;
    }

    wifi_init_config_t initialization =
        WIFI_INIT_CONFIG_DEFAULT();

    result =
        esp_wifi_init(
            &initialization
        );

    if (result != ESP_OK) {
        return result;
    }

    result =
        esp_event_handler_instance_register(
            WIFI_EVENT,
            ESP_EVENT_ANY_ID,
            wifi_manager_event_handler,
            NULL,
            &s_wifi_event_instance
        );

    if (result != ESP_OK) {
        return result;
    }

    result =
        esp_event_handler_instance_register(
            IP_EVENT,
            IP_EVENT_STA_GOT_IP,
            wifi_manager_event_handler,
            NULL,
            &s_ip_event_instance
        );

    if (result != ESP_OK) {
        return result;
    }

    wifi_config_t configuration;

    memset(
        &configuration,
        0,
        sizeof(configuration)
    );

    memcpy(
        configuration.sta.ssid,
        CONFIG_FIRMWARE_WIFI_SSID,
        ssid_length
    );

    memcpy(
        configuration.sta.password,
        CONFIG_FIRMWARE_WIFI_PASSWORD,
        password_length
    );

    configuration.sta.threshold.authmode =
        password_length == 0U
            ? WIFI_AUTH_OPEN
            : WIFI_AUTH_WPA2_PSK;

    result =
        esp_wifi_set_mode(
            WIFI_MODE_STA
        );

    if (result != ESP_OK) {
        return result;
    }

    result =
        esp_wifi_set_config(
            WIFI_IF_STA,
            &configuration
        );

    if (result != ESP_OK) {
        return result;
    }

    s_status.initialized = true;

    ESP_LOGI(
        TAG,
        "Wi-Fi initialized: SSID=%s",
        CONFIG_FIRMWARE_WIFI_SSID
    );

    return ESP_OK;
}

esp_err_t wifi_manager_start(void)
{
    if (!s_status.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (s_status.started) {
        return ESP_ERR_INVALID_STATE;
    }

    xEventGroupClearBits(
        s_event_group,
        WIFI_CONNECTED_BIT |
        WIFI_FAILED_BIT
    );

    s_status.started = true;

    esp_err_t result =
        esp_wifi_start();

    if (result != ESP_OK) {
        s_status.started = false;

        return result;
    }

    ESP_LOGI(
        TAG,
        "Wi-Fi station started"
    );

    return ESP_OK;
}

esp_err_t wifi_manager_stop(void)
{
    if (!s_status.initialized ||
        !s_status.started) {

        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t result =
        esp_wifi_stop();

    if (result != ESP_OK) {
        return result;
    }

    s_status.started = false;
    s_status.connected = false;
    s_status.has_ip = false;
    s_status.retry_count = 0U;
    s_test_force_offline = false;

    xEventGroupClearBits(
        s_event_group,
        WIFI_CONNECTED_BIT |
        WIFI_FAILED_BIT
    );

    ESP_LOGI(
        TAG,
        "Wi-Fi station stopped"
    );

    return ESP_OK;
}

esp_err_t wifi_manager_wait_connected(
    uint32_t timeout_ms
)
{
    if (!s_status.initialized ||
        !s_status.started) {

        return ESP_ERR_INVALID_STATE;
    }

    if (timeout_ms == 0U) {
        return ESP_ERR_INVALID_ARG;
    }

    const EventBits_t bits =
        xEventGroupWaitBits(
            s_event_group,
            WIFI_CONNECTED_BIT |
            WIFI_FAILED_BIT,
            pdFALSE,
            pdFALSE,
            pdMS_TO_TICKS(timeout_ms)
        );

    if ((bits & WIFI_CONNECTED_BIT) != 0U) {
        return ESP_OK;
    }

    if ((bits & WIFI_FAILED_BIT) != 0U) {
        return ESP_FAIL;
    }

    return ESP_ERR_TIMEOUT;
}

esp_err_t wifi_manager_test_force_offline(void)
{
    if (!s_status.initialized ||
        !s_status.started) {

        return ESP_ERR_INVALID_STATE;
    }

    if (s_test_force_offline) {
        return ESP_OK;
    }

    /*
     * Set the flag BEFORE requesting disconnect so the asynchronous
     * WIFI_EVENT_STA_DISCONNECTED handler cannot immediately reconnect.
     */
    s_test_force_offline = true;

    xEventGroupClearBits(
        s_event_group,
        WIFI_CONNECTED_BIT |
        WIFI_FAILED_BIT
    );

    ESP_LOGW(
        TAG,
        "TEST: forcing Wi-Fi offline"
    );

    const esp_err_t result =
        esp_wifi_disconnect();

    if ((result != ESP_OK) &&
        (result != ESP_ERR_WIFI_NOT_CONNECT)) {

        s_test_force_offline = false;

        ESP_LOGE(
            TAG,
            "TEST: forced Wi-Fi disconnect failed: %s",
            esp_err_to_name(result)
        );

        return result;
    }

    /*
     * Update the public state immediately. The disconnect event will confirm
     * the transition asynchronously.
     */
    s_status.connected = false;
    s_status.has_ip = false;

    return ESP_OK;
}


esp_err_t wifi_manager_test_restore_online(void)
{
    if (!s_status.initialized ||
        !s_status.started) {

        return ESP_ERR_INVALID_STATE;
    }

    if (!s_test_force_offline) {
        return ESP_OK;
    }

    /*
     * Clear the test gate BEFORE esp_wifi_connect(). From this point onward,
     * genuine disconnect events use the normal retry/recovery logic again.
     */
    s_test_force_offline = false;
    s_status.retry_count = 0U;

    xEventGroupClearBits(
        s_event_group,
        WIFI_FAILED_BIT
    );

    s_status.connection_attempts++;

    ESP_LOGW(
        TAG,
        "TEST: restoring Wi-Fi connection"
    );

    const esp_err_t result =
        esp_wifi_connect();

    if (result != ESP_OK) {
        ESP_LOGE(
            TAG,
            "TEST: Wi-Fi restore request failed: %s",
            esp_err_to_name(result)
        );

        return result;
    }

    return ESP_OK;
}


bool wifi_manager_test_is_forced_offline(void)
{
    return s_test_force_offline;
}


esp_err_t wifi_manager_get_status(
    wifi_manager_status_t *output
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

bool wifi_manager_is_connected(void)
{
    return
        s_status.initialized &&
        s_status.started &&
        s_status.connected &&
        s_status.has_ip;
}