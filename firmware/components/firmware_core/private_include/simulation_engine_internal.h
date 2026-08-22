#ifndef SIMULATION_ENGINE_INTERNAL_H
#define SIMULATION_ENGINE_INTERNAL_H

#include <stdbool.h>
#include <stdint.h>

#include "../../components/firmware_core/include/app_types.h"

typedef struct {
    signal_id_t signal_id;
    uint64_t next_sample_uptime_ms;
    bool initialized;
} simulation_schedule_entry_t;

typedef struct {
    float room_temperature[4];
    float air_temperature;
    float relative_humidity;
    float compressor_current;
    float battery_voltage;

    bool door_safe;
    bool door_aux;
    bool leak_alarm;
    bool leak_cable_fault;

    bool compressor_run;
    bool compressor_trip;
    bool evaporator_fan_run;
    bool power_failure;
} simulation_process_state_t;

#endif