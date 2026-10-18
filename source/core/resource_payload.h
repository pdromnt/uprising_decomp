#ifndef UPRISING_RESOURCE_PAYLOAD_H
#define UPRISING_RESOURCE_PAYLOAD_H
#include <stddef.h>
#include <stdint.h>

typedef enum ResourcePayloadStatus {
    RESOURCE_PAYLOAD_OK, RESOURCE_PAYLOAD_INVALID_BUFFER,
    RESOURCE_PAYLOAD_ODD_PACKED_SIZE, RESOURCE_PAYLOAD_SIZE_MISMATCH
} ResourcePayloadStatus;
/* Decode CLN count/value pairs into an exact-sized borrowed output span.
 * Buffers must not overlap and remain valid for their sizes. Validation occurs
 * before writing, so rejected input leaves output unchanged. Null is allowed
 * for a zero-sized span. This checked contract excludes retail malformed-input
 * overreads/overwrites and is not a serialization of native object layout. */
ResourcePayloadStatus resource_payload_decode_rle(const uint8_t *packed,
    size_t packed_size, uint8_t *output, size_t output_size);
#endif
