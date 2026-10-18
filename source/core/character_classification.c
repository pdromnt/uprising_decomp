#include "character_classification.h"

uint16_t game_character_classification_table(GameMemory *memory,
    GameAddress table, uint16_t character) {
    return game_memory_read_u16(memory, table + (uint32_t)character * 2u);
}

uint16_t game_character_classification_mask(uint16_t classes,
    uint16_t requested) {
    return (uint16_t)(classes & requested);
}

int game_character_classification_clear(GameMemory *memory,
        GameAddress table, int reverse) {
    unsigned index;
    GameAddress cursor = table;
    if (!memory) return 0;
    for (index = 0u; index < 64u; ++index) {
        game_memory_write_u32(memory, cursor, 0u);
        if (memory->fault) return 0;
        cursor += reverse ? 0xfffffffcu : 4u;
    }
    game_memory_write_u8(memory, cursor, 0u);
    return !memory->fault;
}
