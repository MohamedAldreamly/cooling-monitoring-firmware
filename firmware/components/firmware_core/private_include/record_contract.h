#ifndef RECORD_CONTRACT_H
#define RECORD_CONTRACT_H

#include <stdint.h>

#include "app_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ALARM_RECORD_PAYLOAD_VERSION    1U

typedef struct {
    uint16_t payload_version;
    uint16_t payload_size;

    alarm_event_t alarm_event;
} alarm_record_payload_v1_t;

#ifdef __cplusplus
}
#endif

#endif