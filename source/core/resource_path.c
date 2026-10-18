#include "resource_path.h"

ResourceScanResult resource_path_find_byte(ResourcePathBytes path, uint8_t needle) {
    ResourceScanResult result = {RESOURCE_SCAN_UNREADABLE, 0, path.readable_size};
    size_t offset;
    for (offset = 0; offset < path.readable_size; ++offset) {
        uint8_t value = path.data[offset];
        if (value == needle) {
            result.status = RESOURCE_SCAN_FOUND;
            result.byte_offset = offset;
            return result;
        }
        if (value == 0) {
            result.status = RESOURCE_SCAN_ABSENT;
            return result;
        }
    }
    return result;
}
