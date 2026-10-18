#include "runtime_arena.h"

int game_runtime_arena_clear_large(GameMemory *memory, GameAddress base) {
    uint32_t index;
    if (!memory) return 0;
    for (index = 0u; index < 0x8007u; ++index) {
        game_memory_write_u32(memory, base + index * 4u, 0u);
        if (memory->fault) return 0;
    }
    game_memory_write_u16(memory, base + 0x2001cu, 0u);
    if (memory->fault) return 0;
    game_memory_write_u8(memory, base + 0x2001eu, 0u);
    return !memory->fault;
}
