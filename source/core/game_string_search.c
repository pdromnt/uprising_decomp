#include "game_string_search.h"
#include "resource_path.h"

GameAddress game_string_find_byte(GameMemory *memory, GameAddress string,
    uint8_t needle) {
    for (;;) {
        if (memory->fault) return 0u;
        if (game_memory_contains(memory, string, 1u)) {
            size_t offset = (size_t)(string - memory->base);
            size_t available = memory->size - offset;
            uint64_t before_wrap = (uint64_t)UINT32_MAX - string + 1u;
            ResourcePathBytes path;
            ResourceScanResult result;
            /* Mapping can span the numeric guest-address wrap. Preserve the
             * adapter's uint32 increment before consulting the next mapping. */
            if ((uint64_t)available > before_wrap) available = (size_t)before_wrap;
            path.data = memory->data + offset;
            path.readable_size = available;
            result = resource_path_find_byte(path, needle);
            if (result.status == RESOURCE_SCAN_FOUND)
                return string + (uint32_t)result.byte_offset;
            if (result.status == RESOURCE_SCAN_ABSENT) return 0u;
            string += (uint32_t)available;
            /* End of this span need not be a fault: a fallback mapping may
             * supply the next byte. Only an actual failed read raises it. */
            continue;
        }
        uint8_t value = game_memory_read_u8(memory, string);
        if (memory->fault) return 0u;
        if (value == needle) return string;
        if (value == 0u) return 0u;
        string++;
    }
}
