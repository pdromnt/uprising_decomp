#ifndef UPRISING_NATIVE_ENTITY_TABLES_H
#define UPRISING_NATIVE_ENTITY_TABLES_H

#include "game_memory.h"

int game_entity_table_initialize(GameMemory *memory, GameAddress table,
    uint32_t count);
int game_entity_table_fill(GameMemory *memory, GameAddress table,
    uint32_t count, uint32_t value, int reverse);

#endif
