#include "startup_tables.h"

int game_startup_name_tables_clear(GameMemory *memory, int reverse) {
    static const GameAddress tables[] = {
        0x005a80d8u, 0x005a9848u, 0x005a3a88u
    };
    unsigned i;
    if (!memory) return 0;
    for (i = 0u; i < sizeof(tables) / sizeof(tables[0]); ++i) {
        if (!game_memory_fill_u32(memory, tables[i], 0x5dcu,
                0u, reverse)) return 0;
    }
    return 1;
}
