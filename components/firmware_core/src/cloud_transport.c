#include "cloud_transport.h"

#include <string.h>

#include "cloud_contract.h"
#include "esp_crt_bundle.h"
#include "esp_log.h"
#include "mqtt_client.h"

/*
 * AWS IoT Core configuration
 */

#define CLOUD_AWS_ENDPOINT \
    "a1qmtgjcruon5j-ats.iot.us-east-1.amazonaws.com"

#define CLOUD_MQTT_PORT 8883U

/*
 * These symbols are generated automatically by ESP-IDF when the following
 * files are added to EMBED_TXTFILES in this component CMakeLists.txt:
 *
 *     certs/device_certificate.pem.crt
 *     certs/device_private_key.pem.key
 *
 * EMBED_TXTFILES guarantees a trailing '\0', therefore the buffers can be
 * passed directly to ESP-MQTT as PEM strings.
 */
extern const uint8_t device_certificate_pem_crt_start[]
    asm("_binary_device_certificate_pem_crt_start");

extern const uint8_t device_certificate_pem_crt_end[]
    asm("_binary_device_certificate_pem_crt_end");

extern const uint8_t device_private_key_pem_key_start[]
    asm("_binary_device_private_key_pem_key_start");

extern const uint8_t device_private_key_pem_key_end[]
    asm("_binary_device_private_key_pem_key_end");


static const char *TAG = "CLOUD_TRANSPORT";

static esp_mqtt_client_handle_t s_client = NULL;

static cloud_transport_config_t s_config;
static cloud_transport_status_t s_status;

static cloud_transport_message_callback_t
    s_message_callback = NULL;

static void *s_message_callback_context = NULL;

static cloud_transport_subscription_callback_t
    s_subscription_callback = NULL;

static void *s_subscription_callback_context = NULL;

static char s_ack_payload[
    CLOUD_TRANSPORT_ACK_PAYLOAD_MAX_LEN + 1U
];

static size_t s_ack_payload_received = 0U;
static size_t s_ack_payload_expected = 0U;
static bool s_receiving_ack = false;


/* -------------------------------------------------------------------------- */
/* Internal helpers                                                           */
/* -------------------------------------------------------------------------- */

static void cloud_transport_emit_event(
    cloud_transport_event_type_t type,
    int mqtt_message_id,
    const char *payload,
    size_t payload_length
)
{

    if (s_config.event_callback == NULL) {
        return;
    }

    const cloud_transport_event_t event = {
        .type = type,
        .mqtt_message_id = mqtt_message_id,
        .payload = payload,
        .payload_length = payload_length,
    };

    s_config.event_callback(
        &event,
        s_config.user_context
    );
}

static bool cloud_transport_topic_matches(
    const char *topic,
    int topic_length,
    const char *expected_topic
)
{
    if ((topic == NULL) ||
        (topic_length <= 0) ||
        (expected_topic == NULL)) {

        return false;
    }

    const size_t expected_length =
        strlen(expected_topic);

    if ((size_t)topic_length != expected_length) {
        return false;
    }

    return memcmp(
        topic,
        expected_topic,
        expected_length
    ) == 0;
}


static void cloud_transport_reset_ack_assembly(void)
{
    memset(
        s_ack_payload,
        0,
        sizeof(s_ack_payload)
    );

    s_ack_payload_received = 0U;
    s_ack_payload_expected = 0U;
    s_receiving_ack = false;
}


static void cloud_transport_report_error(void)
{
    s_status.errors++;

    cloud_transport_emit_event(
        CLOUD_TRANSPORT_EVENT_ERROR,
        0,
        NULL,
        0U
    );
}

static void cloud_transport_forward_message(
    const esp_mqtt_event_handle_t event
)
{
    if ((event == NULL) ||
        (s_message_callback == NULL)) {

        return;
    }

    if ((event->topic == NULL) ||
        (event->topic_len <= 0)) {

        return;
    }

    const cloud_transport_message_t message = {
        .topic = event->topic,
        .topic_length =
            (size_t)event->topic_len,

        .payload = event->data,
        .payload_length =
            (event->data_len > 0)
                ? (size_t)event->data_len
                : 0U,

        .total_payload_length =
            (event->total_data_len > 0)
                ? (size_t)event->total_data_len
                : 0U,

        .current_payload_offset =
            (event->current_data_offset > 0)
                ? (size_t)event->current_data_offset
                : 0U,
    };

    s_message_callback(
        &message,
        s_message_callback_context
    );
}

/*
 * Application ACKs may arrive in several MQTT_EVENT_DATA events when the
 * payload is larger than the MQTT receive buffer. We therefore assemble the
 * complete ACK before forwarding it to cloud_uploader.
 */
static void cloud_transport_handle_ack_data(
    const esp_mqtt_event_handle_t event
)
{
    if (event == NULL) {
        return;
    }

    /*
     * The topic is checked when the first fragment arrives.
     */
    if (event->current_data_offset == 0) {
        cloud_transport_reset_ack_assembly();

        if (!cloud_transport_topic_matches(
                event->topic,
                event->topic_len,
                CLOUD_TOPIC_ALARM_ACK)) {

            return;
        }

        if ((event->total_data_len < 0) ||
            ((size_t)event->total_data_len >
             CLOUD_TRANSPORT_ACK_PAYLOAD_MAX_LEN)) {

            ESP_LOGE(
                TAG,
                "Application ACK payload too large: %d bytes",
                event->total_data_len
            );

            cloud_transport_report_error();
            return;
        }

        s_receiving_ack = true;
        s_ack_payload_expected =
            (size_t)event->total_data_len;
    }

    if (!s_receiving_ack) {
        return;
    }

    if ((event->data_len < 0) ||
        (event->current_data_offset < 0)) {

        ESP_LOGE(
            TAG,
            "Invalid ACK fragment"
        );

        cloud_transport_report_error();
        cloud_transport_reset_ack_assembly();

        return;
    }

    const size_t offset =
        (size_t)event->current_data_offset;

    const size_t chunk_length =
        (size_t)event->data_len;

    /*
     * Reject malformed/overflowing fragments.
     */
    if ((offset > s_ack_payload_expected) ||
        (chunk_length >
         (s_ack_payload_expected - offset)) ||
        ((offset + chunk_length) >
         CLOUD_TRANSPORT_ACK_PAYLOAD_MAX_LEN)) {

        ESP_LOGE(
            TAG,
            "Invalid ACK fragment boundaries"
        );

        cloud_transport_report_error();
        cloud_transport_reset_ack_assembly();

        return;
    }

    if ((chunk_length > 0U) &&
        (event->data != NULL)) {

        memcpy(
            &s_ack_payload[offset],
            event->data,
            chunk_length
        );
    }

    /*
     * MQTT fragments are expected in increasing offsets.
     */
    if ((offset + chunk_length) >
        s_ack_payload_received) {

        s_ack_payload_received =
            offset + chunk_length;
    }

    if (s_ack_payload_received <
        s_ack_payload_expected) {

        return;
    }

    s_ack_payload[s_ack_payload_expected] = '\0';

    s_status.application_acks++;

    ESP_LOGI(
        TAG,
        "Application ACK received: %u bytes",
        (unsigned)s_ack_payload_expected
    );

    /*
     * IMPORTANT:
     *
     * This callback only forwards the Application ACK.
     * cloud_transport does NOT advance upload_cursor.
     *
     * cloud_uploader must validate the ACK record_id first, and only then
     * commit next_offset/record_id to the persistent cursor.
     */
    cloud_transport_emit_event(
        CLOUD_TRANSPORT_EVENT_APPLICATION_ACK,
        0,
        s_ack_payload,
        s_ack_payload_expected
    );

    cloud_transport_reset_ack_assembly();
}

/* -------------------------------------------------------------------------- */
/* ESP-MQTT event handler                                                     */
/* -------------------------------------------------------------------------- */

static void cloud_transport_mqtt_event_handler(
    void *handler_args,
    esp_event_base_t base,
    int32_t event_id,
    void *event_data
)
{
    (void)handler_args;
    (void)base;

    esp_mqtt_event_handle_t event =
        (esp_mqtt_event_handle_t)event_data;

    switch ((esp_mqtt_event_id_t)event_id) {

        case MQTT_EVENT_BEFORE_CONNECT:

            s_status.connection_attempts++;

            ESP_LOGI(
                TAG,
                "Connecting to AWS IoT Core: %s:%u",
                CLOUD_AWS_ENDPOINT,
                (unsigned)CLOUD_MQTT_PORT
            );

            break;


        case MQTT_EVENT_CONNECTED: {

            s_status.connected = true;
            s_status.successful_connections++;

            /*
             * Reset subscription state after every fresh
             * MQTT connection.
             */
            s_status.ack_subscription_active = false;

            s_status.ready = false;

            ESP_LOGI(
                TAG,
                "AWS IoT MQTT connected"
            );

            cloud_transport_emit_event(
                CLOUD_TRANSPORT_EVENT_CONNECTED,
                0,
                NULL,
                0U
            );

            /*
             * -------------------------------------------------
             * Application ACK subscription
             * -------------------------------------------------
             *
             * This subscription remains the only subscription
             * that controls cloud_transport READY state.
             */
            const int ack_message_id =
                esp_mqtt_client_subscribe(
                    s_client,
                    CLOUD_TOPIC_ALARM_ACK,
                    CLOUD_MQTT_QOS
                );

            if (ack_message_id < 0) {

                ESP_LOGE(
                    TAG,
                    "Failed to request subscription "
                    "to ACK topic: %s",
                    CLOUD_TOPIC_ALARM_ACK
                );

                cloud_transport_report_error();
                break;
            }

            s_status.subscribe_requests++;

            s_status.ack_subscription_message_id =
                ack_message_id;

            ESP_LOGI(
                TAG,
                "ACK subscription requested: "
                "msg_id=%d topic=%s",
                ack_message_id,
                CLOUD_TOPIC_ALARM_ACK
            );

            break;
        }


        case MQTT_EVENT_DISCONNECTED:

            s_status.connected = false;

            s_status.ack_subscription_active = false;

            s_status.ready = false;

            s_status.disconnections++;

            cloud_transport_reset_ack_assembly();

            ESP_LOGW(
                TAG,
                "AWS IoT MQTT disconnected"
            );

            cloud_transport_emit_event(
                CLOUD_TRANSPORT_EVENT_DISCONNECTED,
                0,
                NULL,
                0U
            );

            break;


        case MQTT_EVENT_SUBSCRIBED:

        s_status.subscribe_acks++;

        /*
        * Alarm ACK subscription confirmed.
        */
        if ((event != NULL) &&
            (event->msg_id ==
            s_status.ack_subscription_message_id)) {

            s_status.ack_subscription_active = true;
            s_status.ready = true;

            ESP_LOGI(
                TAG,
                "Cloud transport READY; ACK topic active"
            );

            cloud_transport_emit_event(
                CLOUD_TRANSPORT_EVENT_READY,
                event->msg_id,
                NULL,
                0U
            );
        }

        /*
        * Notify optional upper-layer consumer about
        * any MQTT subscription acknowledgement.
        */
        if ((event != NULL) &&
            (s_subscription_callback != NULL)) {

            s_subscription_callback(
                event->msg_id,
                s_subscription_callback_context
            );
        }

        break;

        case MQTT_EVENT_PUBLISHED:

            /*
            * QoS 1 MQTT_EVENT_PUBLISHED means broker PUBACK.
            *
            * IMPORTANT:
            * This is not our Application ACK and therefore
            * MUST NOT advance upload_cursor.
            */

            s_status.broker_publish_acks++;

            if (event != NULL) {

                ESP_LOGI(
                    TAG,
                    "Broker PUBACK received: msg_id=%d",
                    event->msg_id
                );

                cloud_transport_emit_event(
                    CLOUD_TRANSPORT_EVENT_BROKER_PUBLISHED,
                    event->msg_id,
                    NULL,
                    0U
                );
            }

            break;

        case MQTT_EVENT_DATA:

        if (event == NULL) {
            break;
        }

        /*
        * Alarm Application ACK keeps its existing,
        * proven processing path.
        */
        if (cloud_transport_topic_matches(
                event->topic,
                event->topic_len,
                CLOUD_TOPIC_ALARM_ACK)) {

            cloud_transport_handle_ack_data(
                event
            );

            break;
        }

        /*
         * Any other MQTT message is forwarded to the
         * optional upper-layer consumer.
         *
         * cloud_transport does not know whether this is
         * Shadow, telemetry, commands, etc.
         */
        cloud_transport_forward_message(
            event
        );

        break;

        case MQTT_EVENT_ERROR:

            ESP_LOGE(
                TAG,
                "MQTT/TLS transport error"
            );

            cloud_transport_report_error();

            break;


        default:
            break;
    }
}

/* -------------------------------------------------------------------------- */
/* Public API                                                                 */
/* -------------------------------------------------------------------------- */

esp_err_t cloud_transport_set_message_callback(
    cloud_transport_message_callback_t callback,
    void *user_context
)
{
    if (!s_status.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    s_message_callback =
        callback;

    s_message_callback_context =
        user_context;

    return ESP_OK;
}

esp_err_t cloud_transport_set_subscription_callback(
    cloud_transport_subscription_callback_t callback,
    void *user_context
)
{
    if (!s_status.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    s_subscription_callback =
        callback;

    s_subscription_callback_context =
        user_context;

    return ESP_OK;
}

esp_err_t cloud_transport_init(
    const cloud_transport_config_t *config
)
{
    if (s_status.initialized ||
        (s_client != NULL)) {

        return ESP_ERR_INVALID_STATE;
    }

    memset(
        &s_status,
        0,
        sizeof(s_status)
    );

    memset(
        &s_config,
        0,
        sizeof(s_config)
    );

    s_message_callback = NULL;
    s_message_callback_context = NULL;

    s_subscription_callback = NULL;
    s_subscription_callback_context = NULL;

    /*
     * Credentials are now owned by this component and embedded from:
     *
     * components/firmware_core/certs/device_certificate.pem.crt
     * components/firmware_core/certs/device_private_key.pem.key
     *
     * Endpoint and Client ID are also defined here.
     *
     * config is therefore optional for those values. It is retained so the
     * upper layer can provide the event callback/user context and, if ever
     * needed, an explicit server root CA.
     */
    s_config.endpoint =
        CLOUD_AWS_ENDPOINT;

    s_config.client_id =
        CLOUD_DEVICE_ID;

    s_config.client_certificate_pem =
        (const char *)
        device_certificate_pem_crt_start;

    s_config.client_private_key_pem =
        (const char *)
        device_private_key_pem_key_start;

    s_config.server_root_ca_pem = NULL;

    if (config != NULL) {
        s_config.event_callback =
            config->event_callback;

        s_config.user_context =
            config->user_context;

        /*
         * Optional manual server CA override.
         * If NULL, ESP-IDF certificate bundle is used.
         */
        s_config.server_root_ca_pem =
            config->server_root_ca_pem;
    }

    /*
     * Sanity check for embedded files.
     */
    const size_t certificate_size =
        (size_t)(
            device_certificate_pem_crt_end -
            device_certificate_pem_crt_start
        );

    const size_t private_key_size =
        (size_t)(
            device_private_key_pem_key_end -
            device_private_key_pem_key_start
        );

    if ((certificate_size <= 1U) ||
        (private_key_size <= 1U)) {

        ESP_LOGE(
            TAG,
            "Embedded AWS certificate/private key is empty"
        );

        memset(
            &s_config,
            0,
            sizeof(s_config)
        );

        return ESP_ERR_INVALID_SIZE;
    }

    ESP_LOGI(
        TAG,
        "Embedded device certificate: %u bytes",
        (unsigned)certificate_size
    );

    /*
     * Never print the private key itself.
     */
    ESP_LOGI(
        TAG,
        "Embedded private key loaded: %u bytes",
        (unsigned)private_key_size
    );

    esp_mqtt_client_config_t mqtt_config = {
        .broker = {
            .address = {
                .hostname = CLOUD_AWS_ENDPOINT,
                .transport = MQTT_TRANSPORT_OVER_SSL,
                .port = CLOUD_MQTT_PORT,
            },
        },

        .credentials = {
            .client_id = CLOUD_DEVICE_ID,

            .authentication = {
                .certificate =
                    (const char *)
                    device_certificate_pem_crt_start,

                .key =
                    (const char *)
                    device_private_key_pem_key_start,
            },
        },

        .session = {
            .disable_clean_session = false,
            .keepalive = 60,
        },

        .network = {
            .reconnect_timeout_ms =
                CLOUD_RETRY_DELAY_MS,
        },
    };

    /*
     * AWS server validation.
     *
     * Normally we use ESP-IDF's built-in certificate bundle.
     * A custom CA can still be supplied from config if needed.
     */
    if ((s_config.server_root_ca_pem != NULL) &&
        (s_config.server_root_ca_pem[0] != '\0')) {

        mqtt_config.broker.verification.certificate =
            s_config.server_root_ca_pem;

        ESP_LOGI(
            TAG,
            "Using explicit server Root CA"
        );

    } else {

        mqtt_config.broker.verification.crt_bundle_attach =
            esp_crt_bundle_attach;

        ESP_LOGI(
            TAG,
            "Using ESP-IDF certificate bundle for AWS server verification"
        );
    }

    s_client =
        esp_mqtt_client_init(
            &mqtt_config
        );

    if (s_client == NULL) {
        ESP_LOGE(
            TAG,
            "esp_mqtt_client_init() failed"
        );

        memset(
            &s_config,
            0,
            sizeof(s_config)
        );

        return ESP_ERR_NO_MEM;
    }

    esp_err_t result =
        esp_mqtt_client_register_event(
            s_client,
            ESP_EVENT_ANY_ID,
            cloud_transport_mqtt_event_handler,
            NULL
        );

    if (result != ESP_OK) {
        ESP_LOGE(
            TAG,
            "Failed to register MQTT event handler: %s",
            esp_err_to_name(result)
        );

        esp_mqtt_client_destroy(
            s_client
        );

        s_client = NULL;

        memset(
            &s_config,
            0,
            sizeof(s_config)
        );

        return result;
    }

    cloud_transport_reset_ack_assembly();

    s_status.initialized = true;

    ESP_LOGI(
        TAG,
        "Cloud transport initialized"
    );

    ESP_LOGI(
        TAG,
        "AWS endpoint: %s",
        CLOUD_AWS_ENDPOINT
    );

    ESP_LOGI(
        TAG,
        "MQTT client ID: %s",
        CLOUD_DEVICE_ID
    );

    return ESP_OK;
}


esp_err_t cloud_transport_start(void)
{
    if (!s_status.initialized ||
        (s_client == NULL)) {

        return ESP_ERR_INVALID_STATE;
    }

    if (s_status.started) {
        return ESP_ERR_INVALID_STATE;
    }

    const esp_err_t result =
        esp_mqtt_client_start(
            s_client
        );

    if (result != ESP_OK) {
        ESP_LOGE(
            TAG,
            "Failed to start MQTT client: %s",
            esp_err_to_name(result)
        );

        return result;
    }

    s_status.started = true;

    ESP_LOGI(
        TAG,
        "MQTT client started"
    );

    return ESP_OK;
}


esp_err_t cloud_transport_stop(void)
{
    if (!s_status.initialized ||
        (s_client == NULL) ||
        !s_status.started) {

        return ESP_ERR_INVALID_STATE;
    }

    const esp_err_t result =
        esp_mqtt_client_stop(
            s_client
        );

    if (result != ESP_OK) {
        ESP_LOGE(
            TAG,
            "Failed to stop MQTT client: %s",
            esp_err_to_name(result)
        );

        return result;
    }

    s_status.started = false;
    s_status.connected = false;

    s_status.ack_subscription_active = false;

    s_status.ready = false;

    cloud_transport_reset_ack_assembly();

    ESP_LOGI(
        TAG,
        "MQTT client stopped"
    );

    return ESP_OK;
}


esp_err_t cloud_transport_deinit(void)
{
    if (!s_status.initialized ||
        (s_client == NULL)) {

        return ESP_ERR_INVALID_STATE;
    }

    if (s_status.started) {
        const esp_err_t stop_result =
            cloud_transport_stop();

        if (stop_result != ESP_OK) {
            return stop_result;
        }
    }

    const esp_err_t result =
        esp_mqtt_client_destroy(
            s_client
        );

    if (result != ESP_OK) {
        ESP_LOGE(
            TAG,
            "Failed to destroy MQTT client: %s",
            esp_err_to_name(result)
        );

        return result;
    }

    s_client = NULL;

    memset(
        &s_config,
        0,
        sizeof(s_config)
    );

    memset(
        &s_status,
        0,
        sizeof(s_status)
    );

    s_subscription_callback = NULL;
    s_subscription_callback_context = NULL;

    cloud_transport_reset_ack_assembly();

    ESP_LOGI(
        TAG,
        "Cloud transport deinitialized"
    );

    return ESP_OK;
}


esp_err_t cloud_transport_publish_alarm(
    const char *json_payload,
    int *out_message_id
)
{
    if ((json_payload == NULL) ||
        (json_payload[0] == '\0')) {

        return ESP_ERR_INVALID_ARG;
    }

    if (!s_status.initialized ||
        !s_status.started ||
        !s_status.connected ||
        !s_status.ready ||
        (s_client == NULL)) {

        return ESP_ERR_INVALID_STATE;
    }

    const size_t payload_length =
        strlen(json_payload);

    if (payload_length >
        CLOUD_ALARM_PAYLOAD_MAX_LEN) {

        ESP_LOGE(
            TAG,
            "Alarm payload too large: %u > %u",
            (unsigned)payload_length,
            (unsigned)CLOUD_ALARM_PAYLOAD_MAX_LEN
        );

        return ESP_ERR_INVALID_SIZE;
    }

    const int message_id =
        esp_mqtt_client_publish(
            s_client,
            CLOUD_TOPIC_ALARM,
            json_payload,
            (int)payload_length,
            CLOUD_MQTT_QOS,
            CLOUD_MQTT_RETAIN
        );

    if (message_id < 0) {
        s_status.errors++;

        ESP_LOGE(
            TAG,
            "Failed to queue alarm publish"
        );

        return ESP_FAIL;
    }

    s_status.publish_requests++;
    s_status.last_publish_message_id =
        message_id;

    if (out_message_id != NULL) {
        *out_message_id =
            message_id;
    }

    ESP_LOGI(
        TAG,
        "Alarm publish queued: msg_id=%d bytes=%u topic=%s",
        message_id,
        (unsigned)payload_length,
        CLOUD_TOPIC_ALARM
    );

    return ESP_OK;
}


esp_err_t cloud_transport_get_status(
    cloud_transport_status_t *output
)
{
    if (output == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!s_status.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    *output =
        s_status;

    return ESP_OK;
}


bool cloud_transport_is_ready(void)
{
    return (
        s_status.initialized &&
        s_status.started &&
        s_status.connected &&
        s_status.ack_subscription_active &&
        s_status.ready
    );
}

esp_err_t cloud_transport_publish_topic(
    const char *topic,
    const char *payload,
    size_t payload_length,
    int qos,
    bool retain,
    int *out_message_id
)
{
    if ((topic == NULL) ||
        (topic[0] == '\0') ||
        (payload == NULL) ||
        (payload_length == 0U)) {

        return ESP_ERR_INVALID_ARG;
    }

    if ((qos < 0) || (qos > 2)) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!s_status.initialized ||
        !s_status.started ||
        !s_status.connected ||
        (s_client == NULL)) {

        return ESP_ERR_INVALID_STATE;
    }

    const int message_id =
        esp_mqtt_client_publish(
            s_client,
            topic,
            payload,
            (int)payload_length,
            qos,
            retain ? 1 : 0
        );

    if (message_id < 0) {

        s_status.errors++;

        ESP_LOGE(
            TAG,
            "Failed to queue MQTT publish: topic=%s",
            topic
        );

        return ESP_FAIL;
    }

    s_status.publish_requests++;
    s_status.last_publish_message_id =
        message_id;

    if (out_message_id != NULL) {
        *out_message_id =
            message_id;
    }

    ESP_LOGI(
        TAG,
        "MQTT publish queued: "
        "msg_id=%d bytes=%u topic=%s",
        message_id,
        (unsigned)payload_length,
        topic
    );

    return ESP_OK;
}

esp_err_t cloud_transport_subscribe_topic(
    const char *topic,
    int qos,
    int *out_message_id
)
{
    if ((topic == NULL) ||
        (topic[0] == '\0')) {

        return ESP_ERR_INVALID_ARG;
    }

    if ((qos < 0) || (qos > 2)) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!s_status.initialized ||
        !s_status.started ||
        !s_status.connected ||
        (s_client == NULL)) {

        return ESP_ERR_INVALID_STATE;
    }

    const int message_id =
        esp_mqtt_client_subscribe(
            s_client,
            topic,
            qos
        );

    if (message_id < 0) {

        s_status.errors++;

        ESP_LOGE(
            TAG,
            "Failed to request MQTT subscription: "
            "topic=%s",
            topic
        );

        return ESP_FAIL;
    }

    s_status.subscribe_requests++;

    if (out_message_id != NULL) {
        *out_message_id =
            message_id;
    }

    ESP_LOGI(
        TAG,
        "MQTT subscription requested: "
        "msg_id=%d topic=%s",
        message_id,
        topic
    );

    return ESP_OK;
}