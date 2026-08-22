#ifndef CLOUD_TRANSPORT_H
#define CLOUD_TRANSPORT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CLOUD_TRANSPORT_ACK_PAYLOAD_MAX_LEN 256U

typedef enum {
    CLOUD_TRANSPORT_EVENT_CONNECTED = 0,
    CLOUD_TRANSPORT_EVENT_DISCONNECTED,
    CLOUD_TRANSPORT_EVENT_READY,
    CLOUD_TRANSPORT_EVENT_BROKER_PUBLISHED,
    CLOUD_TRANSPORT_EVENT_APPLICATION_ACK,
    CLOUD_TRANSPORT_EVENT_ERROR
} cloud_transport_event_type_t;

typedef struct {
    cloud_transport_event_type_t type;

    /*
     * MQTT message identifier for broker-level events.
     * Zero when the event does not relate to a publish packet.
     */
    int mqtt_message_id;

    /*
     * Raw Application ACK payload received from CLOUD_TOPIC_ALARM_ACK.
     * The callback must consume/copy it before returning.
     */
    const char *payload;
    size_t payload_length;
} cloud_transport_event_t;

typedef void (*cloud_transport_event_callback_t)(
    const cloud_transport_event_t *event,
    void *user_context
);

typedef void (*cloud_transport_subscription_callback_t)(
    int mqtt_message_id,
    void *user_context
);

esp_err_t cloud_transport_set_subscription_callback(
    cloud_transport_subscription_callback_t callback,
    void *user_context
);

typedef struct {
    const char *topic;
    size_t topic_length;

    const char *payload;
    size_t payload_length;

    size_t total_payload_length;
    size_t current_payload_offset;
} cloud_transport_message_t;

typedef void (*cloud_transport_message_callback_t)(
    const cloud_transport_message_t *message,
    void *user_context
);

esp_err_t cloud_transport_set_message_callback(
    cloud_transport_message_callback_t callback,
    void *user_context
);

typedef struct {
    /* AWS IoT Core ATS endpoint, without mqtts:// and without a path. */
    const char *endpoint;

    /* MQTT client ID. Normally the AWS IoT Thing name/device ID. */
    const char *client_id;

    /* Device X.509 certificate and private key in PEM format. */
    const char *client_certificate_pem;
    const char *client_private_key_pem;

    /*
     * Optional broker/root CA in PEM format.
     * If NULL, ESP-IDF's built-in certificate bundle is used.
     */
    const char *server_root_ca_pem;

    cloud_transport_event_callback_t event_callback;
    void *user_context;
} cloud_transport_config_t;

typedef struct {
    uint32_t connection_attempts;
    uint32_t successful_connections;
    uint32_t disconnections;
    uint32_t publish_requests;
    uint32_t broker_publish_acks;
    uint32_t application_acks;
    uint32_t subscribe_requests;
    uint32_t subscribe_acks;
    uint32_t errors;

    int last_publish_message_id;
    int ack_subscription_message_id;

    bool initialized;
    bool started;
    bool connected;
    bool ack_subscription_active;
    bool ready;

} cloud_transport_status_t;

/**
 * @brief Initializes the AWS IoT MQTT transport.
 *
 * The provided endpoint/client ID strings and PEM buffers must remain valid
 * for the lifetime of the transport. The function does not copy certificate
 * or private-key contents.
 */
esp_err_t cloud_transport_init(
    const cloud_transport_config_t *config
);

/**
 * @brief Starts the MQTT client asynchronously.
 *
 * Wi-Fi should already be started. Reconnects are handled by ESP-MQTT.
 */
esp_err_t cloud_transport_start(void);

/**
 * @brief Stops the MQTT client.
 */
esp_err_t cloud_transport_stop(void);

/**
 * @brief Deinitializes the MQTT client and clears transport state.
 */
esp_err_t cloud_transport_deinit(void);

/**
 * @brief Publishes one alarm JSON payload using CLOUD_TOPIC_ALARM.
 *
 * QoS/retain settings come from cloud_contract.h. The persistent upload
 * cursor must NOT be advanced merely because this function succeeds or
 * MQTT_EVENT_PUBLISHED is received. Cursor commit belongs to the uploader
 * after a valid Application ACK is received on CLOUD_TOPIC_ALARM_ACK.
 *
 * @param json_payload Null-terminated JSON payload.
 * @param out_message_id Optional returned MQTT message ID.
 */
esp_err_t cloud_transport_publish_alarm(
    const char *json_payload,
    int *out_message_id
);

/**
 * @brief Copies current transport status.
 */
esp_err_t cloud_transport_get_status(
    cloud_transport_status_t *output
);

/**
 * @brief True after MQTT connection and ACK-topic subscription are active.
 */
bool cloud_transport_is_ready(void);

esp_err_t cloud_transport_publish_topic(
    const char *topic,
    const char *payload,
    size_t payload_length,
    int qos,
    bool retain,
    int *out_message_id
);

esp_err_t cloud_transport_subscribe_topic(
    const char *topic,
    int qos,
    int *out_message_id
);

#ifdef __cplusplus
}
#endif

#endif
