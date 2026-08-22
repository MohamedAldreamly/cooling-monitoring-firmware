/*
 * app_types.c
 *
 *  Created on: Aug 4, 2026
 *      Author: Mohamed Aldreamly
 */


#include "app_types.h"

const char *signal_id_to_string(signal_id_t id)
{
    switch (id) {
        case SIGNAL_ID_ROOM_TEMP_01:         return "ROOM_TEMP_01";
        case SIGNAL_ID_ROOM_TEMP_02:         return "ROOM_TEMP_02";
        case SIGNAL_ID_ROOM_TEMP_03:         return "ROOM_TEMP_03";
        case SIGNAL_ID_ROOM_TEMP_04:         return "ROOM_TEMP_04";
        case SIGNAL_ID_AIR_TEMP_01:          return "AIR_TEMP_01";
        case SIGNAL_ID_AIR_RH_01:            return "AIR_RH_01";
        case SIGNAL_ID_DOOR_SAFE:            return "DOOR_SAFE";
        case SIGNAL_ID_DOOR_AUX:             return "DOOR_AUX";
        case SIGNAL_ID_LEAK_ALARM:           return "LEAK_ALARM";
        case SIGNAL_ID_LEAK_CABLE_FAULT:     return "LEAK_CABLE_FAULT";
        case SIGNAL_ID_COMPRESSOR_RUN:       return "COMPRESSOR_RUN";
        case SIGNAL_ID_COMPRESSOR_TRIP:      return "COMPRESSOR_TRIP";
        case SIGNAL_ID_EVAP_FAN_RUN:         return "EVAP_FAN_RUN";
        case SIGNAL_ID_POWER_FAILURE:        return "POWER_FAILURE";
        case SIGNAL_ID_COMPRESSOR_CURRENT:   return "COMPRESSOR_CURRENT";
        case SIGNAL_ID_BATTERY_VOLTAGE:      return "BATTERY_VOLTAGE";
        default:                             return "INVALID";
    }
}

const char *signal_quality_to_string(signal_quality_t quality)
{
    switch (quality) {
        case SIGNAL_QUALITY_GOOD:                  return "good";
        case SIGNAL_QUALITY_UNCERTAIN:             return "uncertain";
        case SIGNAL_QUALITY_BAD:                   return "bad";
        case SIGNAL_QUALITY_STALE:                 return "stale";
        case SIGNAL_QUALITY_OUT_OF_RANGE:          return "out_of_range";
        case SIGNAL_QUALITY_SENSOR_FAULT:          return "sensor_fault";
        case SIGNAL_QUALITY_COMMUNICATION_FAULT:   return "communication_fault";
        case SIGNAL_QUALITY_NOT_AVAILABLE:         return "not_available";
        case SIGNAL_QUALITY_INITIALIZING:          return "initializing";
        case SIGNAL_QUALITY_SUBSTITUTED:           return "substituted";
        default:                                   return "unknown";
    }
}

const char *signal_unit_to_string(signal_unit_t unit)
{
    switch (unit) {
        case SIGNAL_UNIT_NONE:         return "";
        case SIGNAL_UNIT_DEG_C:        return "degC";
        case SIGNAL_UNIT_PERCENT_RH:   return "percentRH";
        case SIGNAL_UNIT_AMPERE:       return "A";
        case SIGNAL_UNIT_VOLT:         return "V";
        default:                       return "";
    }
}

const char *alarm_state_to_string(alarm_state_t state)
{
    switch (state) {
        case ALARM_STATE_NORMAL:      return "normal";
        case ALARM_STATE_PENDING:     return "pending";
        case ALARM_STATE_ACTIVE:      return "active";
        case ALARM_STATE_RETURNED:    return "returned";
        case ALARM_STATE_CLOSED:      return "closed";
        case ALARM_STATE_DEGRADED:    return "degraded";
        default:                      return "unknown";
    }
}

const char *record_type_to_string(record_type_t type)
{
    switch (type) {
        case RECORD_TYPE_TELEMETRY:      return "telemetry";
        case RECORD_TYPE_ALARM:          return "alarm";
        case RECORD_TYPE_EVENT:          return "event";
        case RECORD_TYPE_AUDIT:          return "audit";
        case RECORD_TYPE_HEALTH:         return "health";
        case RECORD_TYPE_DIAGNOSTIC:     return "diagnostic";
        case RECORD_TYPE_CONFIG_RESULT:  return "config_result";
        default:                         return "unknown";
    }
}

