#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t connection_attempts;
    uint32_t disconnections;
    uint32_t successful_connections;
    uint32_t retry_count;

    bool initialized;
    bool started;
    bool connected;
    bool has_ip;
} wifi_manager_status_t;

/**
 * @brief Initializes the TCP/IP stack, default event loop
 *        and Wi-Fi station.
 */
esp_err_t wifi_manager_init(void);

/**
 * @brief Starts Wi-Fi and begins connecting asynchronously.
 */
esp_err_t wifi_manager_start(void);

/**
 * @brief Stops the Wi-Fi station.
 */
esp_err_t wifi_manager_stop(void);

/**
 * @brief Waits until an IP address is obtained.
 *
 * @param timeout_ms Maximum waiting time.
 */
esp_err_t wifi_manager_wait_connected(
    uint32_t timeout_ms
);

/**
 * @brief Copies the current Wi-Fi status.
 */
/**
 * @brief E2E test hook: deliberately disconnects Wi-Fi and suppresses
 *        automatic reconnect while the local firmware keeps running.
 */
esp_err_t wifi_manager_test_force_offline(void);

/**
 * @brief E2E test hook: re-enables normal reconnect behavior and requests
 *        a new Wi-Fi connection.
 */
esp_err_t wifi_manager_test_restore_online(void);

/**
 * @brief Returns true while the deliberate E2E offline gate is active.
 */
bool wifi_manager_test_is_forced_offline(void);

esp_err_t wifi_manager_get_status(
    wifi_manager_status_t *output
);

/**
 * @brief Returns true only after receiving an IP address.
 */
bool wifi_manager_is_connected(void);

#ifdef __cplusplus
}
#endif

#endif