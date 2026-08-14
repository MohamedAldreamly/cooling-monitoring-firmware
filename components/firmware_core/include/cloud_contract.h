#ifndef CLOUD_CONTRACT_H
#define CLOUD_CONTRACT_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CLOUD_SCHEMA_VERSION           1

#define CLOUD_DEVICE_ID                "cooling-unit-01"

#define CLOUD_FIRMWARE_VERSION         "1.0.0"

/*
 * MQTT Topics
 */
#define CLOUD_TOPIC_TELEMETRY          \
    "cooling/" CLOUD_DEVICE_ID "/telemetry"

#define CLOUD_TOPIC_ALARM              \
    "cooling/" CLOUD_DEVICE_ID "/alarm"

#define CLOUD_TOPIC_STATUS             \
    "cooling/" CLOUD_DEVICE_ID "/status"

#define CLOUD_TOPIC_ALARM_ACK \
    "cooling/" CLOUD_DEVICE_ID "/alarm/ack"

/*
 * Alarm publishing configuration.
 */
#define CLOUD_ALARM_PAYLOAD_MAX_LEN         1024U

#define CLOUD_MQTT_QOS                      1
#define CLOUD_MQTT_RETAIN                   0

/*
 * Maximum time to wait for an Application ACK.
 */
#define CLOUD_ACK_TIMEOUT_MS                10000U

/*
 * Delay before retrying a failed alarm upload.
 */
#define CLOUD_RETRY_DELAY_MS                5000U

/*
 * Maximum JSON payload.
 *
 * Telemetry contains all monitored signals,
 * therefore it needs a larger buffer than alarms.
 */
#define CLOUD_MQTT_PAYLOAD_MAX_LEN     4096U

/*
 * Telemetry sent to cloud every 10 seconds.
 *
 * Signal acquisition periods remain unchanged.
 */
#define CLOUD_TELEMETRY_INTERVAL_MS    10000U

#ifdef __cplusplus
}
#endif

#endif