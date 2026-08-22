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
 * Device Status represents the latest health state.
 *
 * QoS 1 requests delivery confirmation from the MQTT broker.
 * Retain 1 keeps the latest device state available for new subscribers.
 */
#define CLOUD_STATUS_QOS              1
#define CLOUD_STATUS_RETAIN           1

/*
 * Device Status is published every 30 seconds while online.
 */
#define CLOUD_STATUS_INTERVAL_MS      30000U

/*
 * Maximum size of one Device Status JSON payload.
 */
#define CLOUD_STATUS_PAYLOAD_MAX_LEN  1024U

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
 * Telemetry represents the latest live state.
 *
 * QoS 0 is used because old Telemetry is not retried or stored.
 * A newer snapshot will replace the missed one.
 */
#define CLOUD_TELEMETRY_QOS            0
#define CLOUD_TELEMETRY_RETAIN         0

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