#ifndef RUNTIME_SHADOW_H
#define RUNTIME_SHADOW_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "app_types.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    bool initialized;
    bool subscriptions_requested;

    bool update_accepted_active;
    bool update_rejected_active;
    bool update_delta_active;

    uint32_t publish_requests;
    uint32_t accepted_updates;
    uint32_t rejected_updates;
    uint32_t delta_messages;
    uint32_t errors;

    uint64_t last_record_id;
} runtime_shadow_status_t;


esp_err_t runtime_shadow_init(void);

esp_err_t runtime_shadow_on_cloud_connected(void);

void runtime_shadow_on_cloud_disconnected(void);

esp_err_t runtime_shadow_update_alarm(
    const durable_record_t *record
);

esp_err_t runtime_shadow_get_status(
    runtime_shadow_status_t *output
);

#ifdef __cplusplus
}
#endif

#endif