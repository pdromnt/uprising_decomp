#include "resource_payload.h"
#include <string.h>

ResourcePayloadStatus resource_payload_decode_rle(const uint8_t *packed,
    size_t packed_size, uint8_t *output, size_t output_size) {
    size_t index, decoded = 0;
    if ((!packed && packed_size) || (!output && output_size))
        return RESOURCE_PAYLOAD_INVALID_BUFFER;
    if (packed_size % 2) return RESOURCE_PAYLOAD_ODD_PACKED_SIZE;
    for (index = 0; index < packed_size; index += 2) {
        size_t count = packed[index];
        /* Subtraction is bounded; do not overflow while summing hostile runs. */
        if (count > output_size - decoded) return RESOURCE_PAYLOAD_SIZE_MISMATCH;
        decoded += count;
    }
    if (decoded != output_size) return RESOURCE_PAYLOAD_SIZE_MISMATCH;
    decoded = 0;
    for (index = 0; index < packed_size; index += 2) {
        size_t count = packed[index];
        if (count) memset(output + decoded, packed[index + 1], count);
        decoded += count;
    }
    return RESOURCE_PAYLOAD_OK;
}
