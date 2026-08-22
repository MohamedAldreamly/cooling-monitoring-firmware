#include "record_decoder.h"

#include <string.h>

#include "record_contract.h"


static bool record_decoder_alarm_event_is_valid(
    const alarm_event_t *event
)
{
    if (event == NULL) {
        return false;
    }

    if ((event->alarm_code[0] == '\0') ||
        (memchr(
            event->alarm_code,
            '\0',
            sizeof(event->alarm_code)
        ) == NULL)) {

        return false;
    }

    if (event->alarm_instance_id == 0U) {
        return false;
    }

    if (event->transition_sequence == 0U) {
        return false;
    }

    if (event->source_signal >= SIGNAL_ID_COUNT) {
        return false;
    }

    if (event->severity >
        ALARM_SEVERITY_CRITICAL) {

        return false;
    }

    if (event->previous_state >
        ALARM_STATE_DEGRADED) {

        return false;
    }

    if (event->current_state >
        ALARM_STATE_DEGRADED) {

        return false;
    }

    if ((event->transition ==
         ALARM_TRANSITION_NONE) ||
        (event->transition >
         ALARM_TRANSITION_DEGRADED)) {

        return false;
    }

    if (event->source_quality >
        SIGNAL_QUALITY_SUBSTITUTED) {

        return false;
    }

    if (event->has_source_value &&
        (event->source_data_type >
         SIGNAL_DATA_TYPE_FLOAT32)) {

        return false;
    }

    return true;
}


esp_err_t record_decoder_decode_alarm(
    const durable_record_t *record,
    decoded_alarm_record_t *output
)
{
    if ((record == NULL) ||
        (output == NULL)) {

        return ESP_ERR_INVALID_ARG;
    }

    if (record->type != RECORD_TYPE_ALARM) {
        return ESP_ERR_INVALID_ARG;
    }

    if (record->payload_length !=
        sizeof(alarm_record_payload_v1_t)) {

        return ESP_ERR_INVALID_SIZE;
    }

    alarm_record_payload_v1_t payload;

    memset(
        &payload,
        0,
        sizeof(payload)
    );

    memcpy(
        &payload,
        record->payload,
        sizeof(payload)
    );

    if (payload.payload_version !=
        ALARM_RECORD_PAYLOAD_VERSION) {

        return ESP_ERR_INVALID_VERSION;
    }

    if (payload.payload_size !=
        sizeof(alarm_record_payload_v1_t)) {

        return ESP_ERR_INVALID_SIZE;
    }

    if (!record_decoder_alarm_event_is_valid(
            &payload.alarm_event)) {

        return ESP_ERR_INVALID_ARG;
    }

    memset(
        output,
        0,
        sizeof(*output)
    );

    output->record =
        record;

    output->alarm_event =
        payload.alarm_event;

    return ESP_OK;
}