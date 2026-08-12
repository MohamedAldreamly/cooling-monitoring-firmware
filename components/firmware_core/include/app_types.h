/*
 * app_types.h
 *
 *  Created on: Aug 4, 2026
 *      Author: Ayman Abu Kareem
 */

#ifndef APP_TYPES_H
#define APP_TYPES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define APP_SIGNAL_ID_MAX_LEN          32
#define APP_ALARM_CODE_MAX_LEN         32
#define APP_RECORD_PAYLOAD_MAX_LEN     512
#define APP_MESSAGE_ID_MAX_LEN         64
#define APP_MAX_SIGNALS_PER_SNAPSHOT   32

typedef enum {
    SIGNAL_ID_ROOM_TEMP_01 = 0,
    SIGNAL_ID_ROOM_TEMP_02,
    SIGNAL_ID_ROOM_TEMP_03,
    SIGNAL_ID_ROOM_TEMP_04,

    SIGNAL_ID_AIR_TEMP_01,
    SIGNAL_ID_AIR_RH_01,

    SIGNAL_ID_DOOR_SAFE,
    SIGNAL_ID_DOOR_AUX,
    SIGNAL_ID_LEAK_ALARM,
    SIGNAL_ID_LEAK_CABLE_FAULT,

    SIGNAL_ID_COMPRESSOR_RUN,
    SIGNAL_ID_COMPRESSOR_TRIP,
    SIGNAL_ID_EVAP_FAN_RUN,
    SIGNAL_ID_POWER_FAILURE,

    SIGNAL_ID_COMPRESSOR_CURRENT,
    SIGNAL_ID_BATTERY_VOLTAGE,

    SIGNAL_ID_COUNT,
    SIGNAL_ID_INVALID = 0xFFFF
} signal_id_t;

typedef enum {
    SIGNAL_DATA_TYPE_BOOL = 0,
    SIGNAL_DATA_TYPE_INT32,
    SIGNAL_DATA_TYPE_UINT32,
    SIGNAL_DATA_TYPE_FLOAT32
} signal_data_type_t;

typedef enum {
    SIGNAL_QUALITY_GOOD = 0,
    SIGNAL_QUALITY_UNCERTAIN,
    SIGNAL_QUALITY_BAD,
    SIGNAL_QUALITY_STALE,
    SIGNAL_QUALITY_OUT_OF_RANGE,
    SIGNAL_QUALITY_SENSOR_FAULT,
    SIGNAL_QUALITY_COMMUNICATION_FAULT,
    SIGNAL_QUALITY_NOT_AVAILABLE,
    SIGNAL_QUALITY_INITIALIZING,
    SIGNAL_QUALITY_SUBSTITUTED
} signal_quality_t;

typedef enum {
    SIGNAL_UNIT_NONE = 0,
    SIGNAL_UNIT_DEG_C,
    SIGNAL_UNIT_PERCENT_RH,
    SIGNAL_UNIT_AMPERE,
    SIGNAL_UNIT_VOLT
} signal_unit_t;

typedef enum {
    RAW_SOURCE_SIMULATOR = 0,
    RAW_SOURCE_MODBUS,
    RAW_SOURCE_DIGITAL_INPUT,
    RAW_SOURCE_ANALOG_INPUT
} raw_source_type_t;

typedef union {
    bool boolean;
    int32_t int32;
    uint32_t uint32;
    float float32;
} signal_data_t;

typedef struct {
    signal_id_t signal_id;
    raw_source_type_t source_type;

    uint16_t source_device_id;
    uint16_t source_channel;
    uint16_t source_register;

    signal_data_type_t raw_data_type;
    signal_data_t raw_value;

    uint64_t uptime_ms;
    int64_t observed_at_ms;

    bool communication_ok;
    bool crc_ok;
    bool source_fault;
} raw_sample_t;

typedef struct {
    signal_id_t signal_id;
    signal_data_type_t data_type;
    signal_data_t value;

    signal_unit_t unit;
    signal_quality_t quality;

    uint64_t uptime_ms;
    int64_t observed_at_ms;

    bool has_value;
} signal_value_t;

typedef struct {
    uint32_t snapshot_sequence;

    uint64_t created_uptime_ms;
    int64_t observed_at_ms;

    size_t signal_count;

    uint32_t good_count;
    uint32_t uncertain_count;
    uint32_t fault_count;
    uint32_t stale_count;
    uint32_t unavailable_count;

    signal_value_t signals[APP_MAX_SIGNALS_PER_SNAPSHOT];
} signal_snapshot_t;

typedef enum {
    ALARM_SEVERITY_INFO = 0,
    ALARM_SEVERITY_WARNING,
    ALARM_SEVERITY_HIGH,
    ALARM_SEVERITY_CRITICAL
} alarm_severity_t;

typedef enum {
    ALARM_STATE_NORMAL = 0,
    ALARM_STATE_PENDING,
    ALARM_STATE_ACTIVE,
    ALARM_STATE_RETURNED,
    ALARM_STATE_CLOSED,
    ALARM_STATE_DEGRADED
} alarm_state_t;

typedef enum {
    ALARM_TRANSITION_NONE = 0,
    ALARM_TRANSITION_PENDING,
    ALARM_TRANSITION_ACTIVATED,
    ALARM_TRANSITION_RETURNED,
    ALARM_TRANSITION_CLOSED,
    ALARM_TRANSITION_DEGRADED
} alarm_transition_t;

typedef struct {
    char alarm_code[APP_ALARM_CODE_MAX_LEN];

    uint64_t alarm_instance_id;
    uint32_t transition_sequence;

    signal_id_t source_signal;

    alarm_severity_t severity;
    alarm_state_t previous_state;
    alarm_state_t current_state;
    alarm_transition_t transition;

    signal_quality_t source_quality;

    signal_data_type_t source_data_type;
    signal_data_t source_value;
    bool has_source_value;

    uint64_t uptime_ms;
    int64_t observed_at_ms;
} alarm_event_t;

typedef enum {
    RECORD_TYPE_TELEMETRY = 0,
    RECORD_TYPE_ALARM,
    RECORD_TYPE_EVENT,
    RECORD_TYPE_AUDIT,
    RECORD_TYPE_HEALTH,
    RECORD_TYPE_DIAGNOSTIC,
    RECORD_TYPE_CONFIG_RESULT
} record_type_t;

typedef enum {
    RECORD_PRIORITY_LOW = 0,
    RECORD_PRIORITY_NORMAL,
    RECORD_PRIORITY_HIGH,
    RECORD_PRIORITY_CRITICAL
} record_priority_t;

typedef struct {
    uint64_t record_id;
    uint32_t boot_id;
    uint32_t boot_sequence;

    record_type_t type;
    record_priority_t priority;

    uint64_t uptime_ms;
    int64_t observed_at_ms;

    bool requires_application_ack;

    uint16_t payload_length;
    uint8_t payload[APP_RECORD_PAYLOAD_MAX_LEN];
} durable_record_t;


const char *signal_id_to_string(signal_id_t id);
const char *signal_quality_to_string(signal_quality_t quality);
const char *signal_unit_to_string(signal_unit_t unit);
const char *alarm_state_to_string(alarm_state_t state);
const char *record_type_to_string(record_type_t type);

#ifdef __cplusplus
}
#endif

#endif



