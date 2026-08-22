#ifndef SYSTEM_IDENTITY_INTERNAL_H
#define SYSTEM_IDENTITY_INTERNAL_H

#define SYSTEM_IDENTITY_NVS_NAMESPACE       "sys_identity"

#define SYSTEM_IDENTITY_KEY_BOOT_ID         "boot_id"
#define SYSTEM_IDENTITY_KEY_RECORD_LIMIT    "record_limit"

/*
 * Record IDs are reserved in blocks.
 *
 * Example:
 * Stored limit = 256
 * Current boot may issue IDs 1..256.
 *
 * On the next reservation, NVS stores 512 and the firmware may
 * issue 257..512.
 *
 * If power is lost, some IDs may be skipped, but no ID is reused.
 */
#define SYSTEM_IDENTITY_RECORD_BLOCK_SIZE   256ULL

#endif