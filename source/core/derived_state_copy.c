#include "derived_state_copy.h"

int game_derived_state_copy_sparse(GameMemory *memory,
        GameAddress destination, GameAddress source) {
    static const uint16_t offsets[] = {
        0x00fcu, 0x01ccu, 0x0274u, 0x0204u, 0x02acu, 0x023cu, 0x02e4u,
        0x0130u, 0x0134u, 0x0138u, 0x013cu, 0x0140u, 0x0144u,
        0x00f0u, 0x00f4u, 0x00f8u,
        0x0148u, 0x014cu, 0x0150u, 0x0154u, 0x0158u, 0x015cu,
        0x0100u, 0x0104u, 0x0108u, 0x010cu, 0x0110u, 0x0114u,
        0x0118u, 0x011cu, 0x0120u, 0x0124u, 0x0128u, 0x012cu,
        0x0160u, 0x0164u
    };
    unsigned index;
    if (!memory) return 0;
    for (index = 0u; index < sizeof(offsets) / sizeof(offsets[0]);
            ++index) {
        uint32_t value = game_memory_read_u32(memory,
            source + offsets[index]);
        if (memory->fault) return 0;
        game_memory_write_u32(memory, destination + offsets[index], value);
        if (memory->fault) return 0;
    }
    return 1;
}
