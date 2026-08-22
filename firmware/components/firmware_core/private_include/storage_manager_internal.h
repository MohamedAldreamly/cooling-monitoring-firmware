#ifndef STORAGE_MANAGER_INTERNAL_H
#define STORAGE_MANAGER_INTERNAL_H

#include <stdint.h>

#define STORAGE_RECORD_MAGIC              0x434F4F4CUL
#define STORAGE_RECORD_FORMAT_VERSION     1U
#define STORAGE_RECORD_COMMIT_MARKER      0x434D4954UL

#define STORAGE_MOUNT_PATH                "/storage"
#define STORAGE_PARTITION_LABEL           "storage"

#define STORAGE_JOURNAL_PATH              \
    STORAGE_MOUNT_PATH "/journal.bin"

#define STORAGE_REPAIR_PATH               \
    STORAGE_MOUNT_PATH "/journal.repair"

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint16_t format_version;
    uint16_t header_size;

    uint16_t record_type;
    uint16_t record_priority;

    uint64_t record_id;
    uint32_t boot_id;
    uint32_t boot_sequence;

    uint64_t uptime_ms;
    int64_t observed_at_ms;

    uint16_t payload_length;
    uint8_t requires_application_ack;
    uint8_t reserved;

    uint32_t header_crc;
} storage_record_header_t;

typedef struct __attribute__((packed)) {
    uint32_t payload_crc;
    uint32_t commit_marker;
} storage_record_footer_t;

#endif