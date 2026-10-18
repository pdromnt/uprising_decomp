#include "sample_pair_registry.h"

GameSamplePairSelection game_sample_pair_register(GameMemory *memory,
    int32_t requested, uint32_t first, uint32_t second) {
    GameSamplePairSelection result;
    uint32_t slot;
    result.index = -1;
    result.used_search = requested < 0 || requested >= 5;
    if (!memory) return result;
    if (!result.used_search) {
        result.index = requested;
    } else {
        for (slot = 0u; slot < 5u; ++slot) {
            uint32_t address = 0x00602538u + slot * 8u;
            uint32_t value = game_memory_read_u32(memory, address);
            if (memory->fault) return result;
            if (value != 0xbf800000u) continue;
            value = game_memory_read_u32(memory, address + 4u);
            if (memory->fault) return result;
            if (value == 0xbf800000u) {
                result.index = (int32_t)slot;
                break;
            }
        }
    }
    if (result.index >= 0) {
        uint32_t address = 0x00602538u + (uint32_t)result.index * 8u;
        game_memory_write_u32(memory, address, first);
        if (!memory->fault)
            game_memory_write_u32(memory, address + 4u, second);
    }
    return result;
}
