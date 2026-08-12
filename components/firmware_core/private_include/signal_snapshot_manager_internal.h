#ifndef SIGNAL_SNAPSHOT_MANAGER_INTERNAL_H
#define SIGNAL_SNAPSHOT_MANAGER_INTERNAL_H

#include <stdbool.h>
#include <stdint.h>

#include "app_types.h"

typedef struct {
    signal_value_t value;

    uint64_t last_update_uptime_ms;

    bool received_once;
    bool stale_reported;
} latest_signal_entry_t;

#endif