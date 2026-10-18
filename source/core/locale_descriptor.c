#include "locale_descriptor.h"

int game_locale_descriptor_clear(GameMemory *memory,
        GameAddress workspace, int reverse) {
    uint32_t i;
    GameAddress cursor = workspace;
    uint32_t word_step = reverse ? 0xfffffffcu : 4u;
    if (!memory) return 0;
    for (i = 0u; i < 0x3du; ++i) {
        game_memory_write_u32(memory, cursor, 0u);
        if (memory->fault) return 0;
        cursor += word_step;
    }
    game_memory_write_u16(memory, cursor, 0u);
    if (memory->fault) return 0;
    cursor += reverse ? 0xfffffffeu : 2u;
    game_memory_write_u8(memory, cursor, 0u);
    return !memory->fault;
}
