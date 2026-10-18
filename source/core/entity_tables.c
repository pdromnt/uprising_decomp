#include "entity_tables.h"

int game_entity_table_initialize(GameMemory *memory, GameAddress table,
        uint32_t count) {
    return game_entity_table_fill(memory, table, count, 0u, 0);
}

int game_entity_table_fill(GameMemory *memory, GameAddress table,
        uint32_t count, uint32_t value, int reverse) {
    return game_memory_fill_u32(memory, table, count, value, reverse);
}
