#include "runtime_nodes.h"

int game_runtime_node_reset_links(GameMemory *memory, GameAddress node) {
    if (!memory) return 0;
    game_memory_write_u32(memory, node + 8u, 0u);
    if (memory->fault) return 0;
    game_memory_write_u32(memory, node + 4u, 0u);
    return !memory->fault;
}

int game_runtime_node_initialize_fields(GameMemory *memory,
        GameAddress node) {
    static const uint8_t offsets[] = {
        0x28u, 0x10u, 0x14u, 0x24u, 0x2cu
    };
    unsigned i;
    if (!memory) return 0;
    game_memory_write_u32(memory, node, 0x005749b0u);
    if (memory->fault) return 0;
    game_memory_write_u32(memory, node + offsets[0], 0u);
    if (memory->fault) return 0;
    game_memory_write_u32(memory, node + 0x0cu, 3u);
    if (memory->fault) return 0;
    for (i = 1u; i < sizeof(offsets) / sizeof(offsets[0]); ++i) {
        game_memory_write_u32(memory, node + offsets[i], 0u);
        if (memory->fault) return 0;
    }
    return 1;
}

int game_runtime_node_initialize(GameMemory *memory, GameAddress node) {
    if (!memory) return 0;
    game_memory_write_u32(memory, node, 0x00572230u);
    if (memory->fault || !game_runtime_node_reset_links(memory, node))
        return 0;
    return game_runtime_node_initialize_fields(memory, node);
}

int game_runtime_pool_clear(GameMemory *memory, GameAddress owner,
        int reverse) {
    uint32_t i;
    if (!memory) return 0;
    for (i = 0u; i < 30u; ++i) {
        game_memory_write_u32(memory, owner + 0x10u +
            i * (reverse ? 0xfffffffcu : 4u), 0u);
        if (memory->fault) return 0;
    }
    return 1;
}
