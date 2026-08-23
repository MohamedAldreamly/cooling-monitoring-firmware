#ifndef ALARM_ENGINE_INTERNAL_H
#define ALARM_ENGINE_INTERNAL_H

#include <stdbool.h>
#include <stdint.h>

#include "alarm_engine.h"

typedef struct {
    alarm_state_t state;

    uint64_t alarm_instance_id;
    uint32_t transition_sequence;

    uint64_t pending_since_ms;
    uint64_t returned_since_ms;

    alarm_severity_t current_severity;

    bool condition_active;
    bool quality_degraded;
} alarm_runtime_state_t;

#endif
