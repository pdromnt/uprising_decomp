#ifndef UPRISING_RESOURCE_PATH_H
#define UPRISING_RESOURCE_PATH_H
#include <stddef.h>
#include <stdint.h>

/* Borrowed immutable bytes, valid for the call. Size is the readable extent,
 * not strlen; a terminator is deliberately not required before scanning. */
typedef struct ResourcePathBytes {
    const uint8_t *data;
    size_t readable_size;
} ResourcePathBytes;
typedef enum ResourceScanStatus {
    RESOURCE_SCAN_FOUND, RESOURCE_SCAN_ABSENT, RESOURCE_SCAN_UNREADABLE
} ResourceScanStatus;
typedef struct ResourceScanResult {
    ResourceScanStatus status;
    size_t byte_offset;  /* valid for FOUND */
    size_t fault_offset; /* valid for UNREADABLE */
} ResourceScanResult;

ResourceScanResult resource_path_find_byte(ResourcePathBytes path, uint8_t needle);
#endif
