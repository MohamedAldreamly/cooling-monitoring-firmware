#ifndef CLOUD_UPLOADER_H
#define CLOUD_UPLOADER_H

#include <stdbool.h>
#include <stdint.h>

#include "cloud_transport.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t records_read;
    uint32_t payloads_encoded;
    uint32_t empty_journal_checks;
    uint32_t read_failures;
    uint32_t unsupported_records;
    uint32_t encoding_failures;

    uint32_t transport_not_ready_checks;
    uint32_t publish_requests;
    uint32_t publishes_queued;
    uint32_t publish_failures;

    uint32_t broker_publish_acks;
    uint32_t application_acks_received;
    uint32_t application_acks_rejected;
    uint32_t ack_timeouts;

    uint32_t cursor_commits;
    uint32_t cursor_commit_failures;

    uint64_t current_record_id;
    uint64_t current_record_offset;
    uint64_t current_next_offset;

    int current_mqtt_message_id;

    bool initialized;
    bool running;
    bool record_waiting_for_ack;
} cloud_uploader_status_t;

esp_err_t cloud_uploader_init(void);
esp_err_t cloud_uploader_start(void);
esp_err_t cloud_uploader_stop(void);

esp_err_t cloud_uploader_get_status(
    cloud_uploader_status_t *output
);

bool cloud_uploader_is_running(void);

/*
 * Pass this callback to cloud_transport_init().
 *
 * Expected Application ACK:
 * {"record_id":123,"status":"ok"}
 */
void cloud_uploader_transport_event_callback(
    const cloud_transport_event_t *event,
    void *user_context
);

#ifdef __cplusplus
}
#endif

#endif
