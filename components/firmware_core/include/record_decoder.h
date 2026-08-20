#ifndef RECORD_DECODER_H
#define RECORD_DECODER_H

#include "app_types.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const durable_record_t *record;

    alarm_event_t alarm_event;
} decoded_alarm_record_t;


/**
 * @brief Decode and validate an Alarm durable record.
 *
 * The durable_record_t is treated as the source of truth.
 *
 * @param record  Durable Alarm record.
 * @param output  Decoded Alarm information.
 *
 * @return
 *     ESP_OK
 *     ESP_ERR_INVALID_ARG
 *     ESP_ERR_INVALID_SIZE
 *     ESP_ERR_INVALID_VERSION
 */
esp_err_t record_decoder_decode_alarm(
    const durable_record_t *record,
    decoded_alarm_record_t *output
);

#ifdef __cplusplus
}
#endif

#endif