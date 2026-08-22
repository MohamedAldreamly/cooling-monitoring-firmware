#include "runtime_shadow.h"

#include <stdio.h>
#include <string.h>

#include "cloud_contract.h"
#include "cloud_transport.h"
#include "esp_log.h"

#include "record_decoder.h"

#define RUNTIME_SHADOW_NAME "runtime"

#define RUNTIME_SHADOW_TOPIC_UPDATE \
    "$aws/things/" CLOUD_DEVICE_ID \
    "/shadow/name/" RUNTIME_SHADOW_NAME \
    "/update"

#define RUNTIME_SHADOW_TOPIC_UPDATE_ACCEPTED \
    "$aws/things/" CLOUD_DEVICE_ID \
    "/shadow/name/" RUNTIME_SHADOW_NAME \
    "/update/accepted"

#define RUNTIME_SHADOW_TOPIC_UPDATE_REJECTED \
    "$aws/things/" CLOUD_DEVICE_ID \
    "/shadow/name/" RUNTIME_SHADOW_NAME \
    "/update/rejected"

#define RUNTIME_SHADOW_TOPIC_UPDATE_DELTA \
    "$aws/things/" CLOUD_DEVICE_ID \
    "/shadow/name/" RUNTIME_SHADOW_NAME \
    "/update/delta"


#define RUNTIME_SHADOW_QOS 1

#define RUNTIME_SHADOW_PAYLOAD_MAX_LEN 512U

static const char *TAG = "RUNTIME_SHADOW";

static runtime_shadow_status_t s_status;

static char s_latest_alarm_payload[
    RUNTIME_SHADOW_PAYLOAD_MAX_LEN
];

static size_t s_latest_alarm_payload_length = 0U;

static bool s_latest_alarm_available = false;

/*
 * true = latest local state has not yet been
 * confirmed by AWS Shadow.
 */
static bool s_alarm_dirty = false;

/*
 * Only one Shadow update is kept in flight.
 * If a newer Alarm arrives while waiting for accepted,
 * we cache it and publish it afterwards.
 */
static bool s_publish_pending = false;

static int s_update_accepted_sub_msg_id = -1;
static int s_update_rejected_sub_msg_id = -1;
static int s_update_delta_sub_msg_id = -1;

static bool runtime_shadow_topic_matches(
    const char *topic,
    size_t topic_length,
    const char *expected_topic
)
{
    if ((topic == NULL) ||
        (expected_topic == NULL)) {

        return false;
    }

    const size_t expected_length =
        strlen(expected_topic);

    if (topic_length != expected_length) {
        return false;
    }

    return memcmp(
        topic,
        expected_topic,
        expected_length
    ) == 0;
}

static esp_err_t runtime_shadow_publish_latest_alarm(void)
{
    if (!s_status.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (!s_latest_alarm_available ||
        !s_alarm_dirty) {

        return ESP_OK;
    }

    /*
     * Do not send another Shadow update while one
     * is waiting for accepted/rejected.
     */
    if (s_publish_pending) {
        return ESP_OK;
    }

    int message_id = -1;

    const esp_err_t result =
        cloud_transport_publish_topic(
            RUNTIME_SHADOW_TOPIC_UPDATE,
            s_latest_alarm_payload,
            s_latest_alarm_payload_length,
            RUNTIME_SHADOW_QOS,
            false,
            &message_id
        );

    if (result != ESP_OK) {

        /*
         * Keep s_alarm_dirty=true.
         * We will retry after reconnect or another opportunity.
         */
        return result;
    }

    s_publish_pending = true;

    /*
     * This exact cached version has now been queued.
     * If another Alarm arrives before AWS accepts it,
     * runtime_shadow_update_alarm() will set dirty=true again.
     */
    s_alarm_dirty = false;

    s_status.publish_requests++;

    ESP_LOGI(
        TAG,
        "Latest Alarm Shadow queued: "
        "record_id=%llu msg_id=%d",
        (unsigned long long)s_status.last_record_id,
        message_id
    );

    return ESP_OK;
}

static void runtime_shadow_handle_subscription_ack(
    int mqtt_message_id,
    void *user_context
)
{
    (void)user_context;

    if (!s_status.initialized) {
        return;
    }

    if (mqtt_message_id ==
        s_update_accepted_sub_msg_id) {

        s_status.update_accepted_active = true;

        ESP_LOGI(
            TAG,
            "Shadow update/accepted subscription active"
        );
    }

    if (mqtt_message_id ==
        s_update_rejected_sub_msg_id) {

        s_status.update_rejected_active = true;

        ESP_LOGI(
            TAG,
            "Shadow update/rejected subscription active"
        );
    }

    if (mqtt_message_id ==
        s_update_delta_sub_msg_id) {

        s_status.update_delta_active = true;

        ESP_LOGI(
            TAG,
            "Shadow update/delta subscription active"
        );
    }

    /*
     * Publish only after ALL Shadow subscriptions
     * are confirmed by the broker.
     */
    if (s_status.update_accepted_active &&
        s_status.update_rejected_active &&
        s_status.update_delta_active) {

        ESP_LOGI(
            TAG,
            "Runtime Shadow subscriptions ready"
        );

        const esp_err_t result =
            runtime_shadow_publish_latest_alarm();

        if ((result != ESP_OK) &&
            (result != ESP_ERR_INVALID_STATE)) {

            s_status.errors++;

            ESP_LOGW(
                TAG,
                "Failed publishing cached Shadow state: %s",
                esp_err_to_name(result)
            );
        }
    }
}

static void runtime_shadow_handle_message(
    const cloud_transport_message_t *message,
    void *user_context
)
{
    (void)user_context;

    if (message == NULL) {
        return;
    }

    if (runtime_shadow_topic_matches(
            message->topic,
            message->topic_length,
            RUNTIME_SHADOW_TOPIC_UPDATE_ACCEPTED)) {

        s_status.accepted_updates++;

        /*
        * The update currently in flight has been accepted.
        */
        s_publish_pending = false;

        ESP_LOGI(
            TAG,
            "Shadow update accepted: %.*s",
            (int)message->payload_length,
            message->payload
        );

        /*
        * A newer Alarm may have arrived while the previous
        * Shadow update was in flight.
        *
        * If so, s_alarm_dirty is already true and we now
        * publish the newest cached state.
        */
        const esp_err_t publish_result =
            runtime_shadow_publish_latest_alarm();

        if ((publish_result != ESP_OK) &&
            (publish_result != ESP_ERR_INVALID_STATE)) {

            s_status.errors++;

            ESP_LOGW(
                TAG,
                "Failed publishing newer cached Shadow state: %s",
                esp_err_to_name(publish_result)
            );
        }

        return;
    }

    if (runtime_shadow_topic_matches(
            message->topic,
            message->topic_length,
            RUNTIME_SHADOW_TOPIC_UPDATE_REJECTED)) {

        s_status.rejected_updates++;
        s_status.errors++;

        s_publish_pending = false;

        /*
        * The latest state must remain dirty so it can
        * be retried later.
        */
        s_alarm_dirty = true;

        ESP_LOGW(
            TAG,
            "Shadow update rejected: %.*s",
            (int)message->payload_length,
            message->payload
        );

        return;
    }

    if (runtime_shadow_topic_matches(
            message->topic,
            message->topic_length,
            RUNTIME_SHADOW_TOPIC_UPDATE_DELTA)) {

        s_status.delta_messages++;

        ESP_LOGI(
            TAG,
            "Shadow delta received: %.*s",
            (int)message->payload_length,
            message->payload
        );

        /*
         * Delta processing will be implemented later.
         */
        return;
    }
}


esp_err_t runtime_shadow_init(void)
{
    memset(
        &s_status,
        0,
        sizeof(s_status)
    );

    memset(
        s_latest_alarm_payload,
        0,
        sizeof(s_latest_alarm_payload)
    );

    s_latest_alarm_payload_length = 0U;
    s_latest_alarm_available = false;
    s_alarm_dirty = false;
    s_publish_pending = false;

    s_update_accepted_sub_msg_id = -1;
    s_update_rejected_sub_msg_id = -1;
    s_update_delta_sub_msg_id = -1;

    const esp_err_t callback_result =
        cloud_transport_set_message_callback(
            runtime_shadow_handle_message,
            NULL
        );

    const esp_err_t subscription_callback_result =
        cloud_transport_set_subscription_callback(
            runtime_shadow_handle_subscription_ack,
            NULL
        );

    if (subscription_callback_result != ESP_OK) {

        ESP_LOGE(
            TAG,
            "Failed to register subscription callback: %s",
            esp_err_to_name(
                subscription_callback_result
            )
        );

        return subscription_callback_result;
    }

    if (callback_result != ESP_OK) {

        ESP_LOGE(
            TAG,
            "Failed to register transport callback: %s",
            esp_err_to_name(callback_result)
        );

        return callback_result;
    }

    s_status.initialized = true;

    ESP_LOGI(
        TAG,
        "Runtime Shadow initialized"
    );

    return ESP_OK;
}


esp_err_t runtime_shadow_on_cloud_connected(void)
{
    if (!s_status.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    s_status.update_accepted_active = false;
    s_status.update_rejected_active = false;
    s_status.update_delta_active = false;

    s_update_accepted_sub_msg_id = -1;
    s_update_rejected_sub_msg_id = -1;
    s_update_delta_sub_msg_id = -1;

    esp_err_t result =
    cloud_transport_subscribe_topic(
        RUNTIME_SHADOW_TOPIC_UPDATE_ACCEPTED,
        RUNTIME_SHADOW_QOS,
        &s_update_accepted_sub_msg_id
    );

    if (result != ESP_OK) {
        s_status.errors++;
        return result;
    }

    ESP_LOGI(
    TAG,
    "Subscribed request to update/accepted: msg_id=%d",
    s_update_accepted_sub_msg_id
);

    result =
        cloud_transport_subscribe_topic(
            RUNTIME_SHADOW_TOPIC_UPDATE_REJECTED,
            RUNTIME_SHADOW_QOS,
            &s_update_rejected_sub_msg_id
        );

    if (result != ESP_OK) {
        s_status.errors++;
        return result;
    }

    ESP_LOGI(
        TAG,
        "Subscribed request to update/rejected: msg_id=%d",
        s_update_rejected_sub_msg_id
    );

    result =
        cloud_transport_subscribe_topic(
            RUNTIME_SHADOW_TOPIC_UPDATE_DELTA,
            RUNTIME_SHADOW_QOS,
            &s_update_delta_sub_msg_id
        );

    if (result != ESP_OK) {
        s_status.errors++;
        return result;
    }

    ESP_LOGI(
        TAG,
        "Subscribed request to update/delta: msg_id=%d",
        s_update_delta_sub_msg_id
    );

    s_status.subscriptions_requested = true;

    return ESP_OK;
}

void runtime_shadow_on_cloud_disconnected(void)
{
    if (!s_status.initialized) {
        return;
    }

    /*
     * MQTT connection is gone.
     * Any Shadow publish that was waiting for an
     * accepted/rejected response can no longer be
     * considered in flight.
     */
    s_publish_pending = false;

    /*
     * Shadow subscriptions belong to the previous
     * MQTT session and must be requested again after
     * reconnect.
     */
    s_status.subscriptions_requested = false;

    s_status.update_accepted_active = false;
    s_status.update_rejected_active = false;
    s_status.update_delta_active = false;

    s_update_accepted_sub_msg_id = -1;
    s_update_rejected_sub_msg_id = -1;
    s_update_delta_sub_msg_id = -1;

    /*
     * If we have a known Alarm state, mark it dirty.
     * On reconnect, only the newest cached state
     * will be published again.
     */
    if (s_latest_alarm_available) {
        s_alarm_dirty = true;
    }

    ESP_LOGI(
        TAG,
        "Cloud disconnected; latest Shadow state marked for republish"
    );
}

esp_err_t runtime_shadow_update_alarm(
    const durable_record_t *record
)
{
    if (record == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!s_status.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    decoded_alarm_record_t decoded;

    const esp_err_t decode_result =
        record_decoder_decode_alarm(
            record,
            &decoded
        );

    if (decode_result != ESP_OK) {

        s_status.errors++;

        ESP_LOGW(
            TAG,
            "Failed decoding Alarm record: "
            "record_id=%llu error=%s",
            (unsigned long long)record->record_id,
            esp_err_to_name(decode_result)
        );

        return decode_result;
    }

    const alarm_event_t *event =
        &decoded.alarm_event;

    /*
     * Build the newest runtime representation regardless
     * of whether AWS is currently connected.
     *
     * This is the key difference from the previous design:
     * Shadow state is cached first, published second.
     */
    const bool active =
        (event->current_state == ALARM_STATE_PENDING) ||
        (event->current_state == ALARM_STATE_ACTIVE) ||
        (event->current_state == ALARM_STATE_DEGRADED);

    const int written =
        snprintf(
            s_latest_alarm_payload,
            sizeof(s_latest_alarm_payload),
            "{"
                "\"state\":{"
                    "\"reported\":{"
                        "\"alarm\":{"
                            "\"record_id\":%llu,"
                            "\"code\":\"%s\","
                            "\"active\":%s,"
                            "\"instance_id\":%llu,"
                            "\"transition_sequence\":%lu,"
                            "\"transition\":%d,"
                            "\"state\":%d,"
                            "\"severity\":%d"
                        "}"
                    "}"
                "}"
            "}",
            (unsigned long long)record->record_id,
            event->alarm_code,
            active ? "true" : "false",
            (unsigned long long)event->alarm_instance_id,
            (unsigned long)event->transition_sequence,
            (int)event->transition,
            (int)event->current_state,
            (int)event->severity
        );

    if ((written <= 0) ||
        ((size_t)written >=
         sizeof(s_latest_alarm_payload))) {

        s_status.errors++;

        return ESP_ERR_INVALID_SIZE;
    }

    s_latest_alarm_payload_length =
        (size_t)written;

    s_latest_alarm_available = true;

    /*
     * Local runtime state is now newer than the state
     * confirmed in AWS.
     */
    s_alarm_dirty = true;

    s_status.last_record_id =
        record->record_id;

    /*
     * Try immediately.
     *
     * If transport is offline, the cached state remains
     * dirty and will be republished after reconnect.
     */
    const esp_err_t result =
        runtime_shadow_publish_latest_alarm();

    if (result == ESP_ERR_INVALID_STATE) {

        ESP_LOGI(
            TAG,
            "Alarm Shadow cached locally: "
            "record_id=%llu cloud offline",
            (unsigned long long)record->record_id
        );

        return ESP_OK;
    }

    if (result != ESP_OK) {

        s_status.errors++;

        ESP_LOGW(
            TAG,
            "Alarm Shadow publish deferred: "
            "record_id=%llu error=%s",
            (unsigned long long)record->record_id,
            esp_err_to_name(result)
        );

        /*
         * Never break durable Alarm processing.
         */
        return ESP_OK;
    }

    return ESP_OK;
}

esp_err_t runtime_shadow_get_status(
    runtime_shadow_status_t *output
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