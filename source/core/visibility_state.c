#include "visibility_state.h"

int game_visibility_state_initialize_begin(GameMemory *memory,
        GameAddress object) {
    if (!memory) return 0;
    game_memory_write_u32(memory, object + 0x1804u, 0u);
    if (memory->fault) return 0;
    game_memory_write_u32(memory, object + 0x1800u, 1u);
    if (memory->fault) return 0;
    game_memory_write_u32(memory, object + 0x11830u, 0x40800000u);
    return !memory->fault;
}

int game_visibility_state_initialize_limits(GameMemory *memory,
        GameAddress object) {
    if (!memory) return 0;
    game_memory_write_u32(memory, object + 0x1808u, 0x7f7fffffu);
    if (memory->fault) return 0;
    game_memory_write_u32(memory, object + 0x180cu, 0x7fffffffu);
    if (memory->fault) return 0;
    game_memory_write_u32(memory, object + 0x12038u, 15u);
    return !memory->fault;
}
