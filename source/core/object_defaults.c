#include "object_defaults.h"

int game_object_clear_default_ranges(GameMemory *memory,
        GameAddress object, int reverse) {
    static const uint16_t offsets[] = {
        0x340u, 0x3f8u, 0x3c0u, 0x378u,
        0xda0u, 0x2ccu, 0x4a0u
    };
    unsigned range;
    if (!memory) return 0;
    for (range = 0u; range < sizeof(offsets) / sizeof(offsets[0]);
            ++range) {
        uint32_t count = range == 5u ? 16u : 14u;
        uint32_t i;
        for (i = 0u; i < count; ++i) {
            game_memory_write_u32(memory, object + offsets[range] +
                i * (reverse ? 0xfffffffcu : 4u), 0u);
            if (memory->fault) return 0;
        }
    }
    return 1;
}
