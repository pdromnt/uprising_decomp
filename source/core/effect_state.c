#include "effect_state.h"

int game_random_effect_begin(GameMemory *memory, GameAddress object) {
    if (!memory) return 0;
    game_memory_write_u32(memory, object + 0xc8u, 0u);
    if (memory->fault) return 0;
    game_memory_write_u32(memory, object + 0x38u, 0x45000000u);
    return !memory->fault;
}

int game_random_effect_clear_vectors(GameMemory *memory,
        GameAddress object) {
    static const uint16_t offsets[] = {
        0xd8u, 0xe0u, 0xfcu, 0x104u
    };
    unsigned i;
    if (!memory) return 0;
    for (i = 0u; i < sizeof(offsets) / sizeof(offsets[0]); ++i) {
        game_memory_write_u32(memory, object + offsets[i], 0u);
        if (memory->fault) return 0;
    }
    return 1;
}

int game_effect_pool_node_finish(GameMemory *memory, GameAddress node) {
    static const uint8_t offsets[] = {4u, 0x0cu, 0x10u, 0x14u, 0x18u};
    unsigned i;
    if (!memory) return 0;
    game_memory_write_u32(memory, node, 0x005740e0u);
    if (memory->fault) return 0;
    for (i = 0u; i < sizeof(offsets) / sizeof(offsets[0]); ++i) {
        game_memory_write_u32(memory, node + offsets[i], 0u);
        if (memory->fault) return 0;
    }
    return 1;
}

int game_effect_resource_initialize(GameMemory *memory, GameAddress node,
        uint16_t object_kind) {
    static const uint8_t long_fields[] = {
        0x10u, 0x14u, 0x18u, 0x1cu, 0x20u, 0x24u, 0x28u,
        0x30u, 0x34u, 0x3cu
    };
    unsigned i;
    if (!memory) return 0;
    if (object_kind == 0u) return game_effect_pool_node_finish(memory, node);
    if(object_kind>=1u && object_kind<=3u){
        static const GameAddress tables[]={0u,0x00574108u,0x005740f8u,0x005740e8u};
        game_memory_write_u32(memory,node,tables[object_kind]);
        game_memory_write_u32(memory,node+4u,0u);
        game_memory_write_u8(memory,node+0x11u,(uint8_t)(object_kind-1u));
        return game_effect_resource_reset(memory,node,object_kind);
    }
    if (object_kind != 5u) return 0;
    game_memory_write_u32(memory, node, 0x00572db8u);
    game_memory_write_u32(memory, node + 4u, 0u);
    game_memory_write_u8(memory, node + 0x0cu, 0u);
    game_memory_write_u16(memory, node + 0x2cu, 0u);
    game_memory_write_u16(memory, node + 0x38u, 1u);
    for (i = 0; i < sizeof(long_fields); ++i)
        game_memory_write_u32(memory, node + long_fields[i], 0u);
    return !memory->fault;
}

int game_effect_resource_reset(GameMemory *memory, GameAddress node,
        uint16_t object_kind) {
    static const uint8_t short_fields[] = {0x0cu, 0x10u, 0x14u, 0x18u};
    static const uint8_t long_fields[] = {
        0x10u, 0x14u, 0x18u, 0x1cu, 0x20u, 0x24u, 0x28u,
        0x30u, 0x34u, 0x3cu
    };
    unsigned i;
    if (!memory) return 0;
    if (object_kind == 0u) {
        for (i = 0; i < sizeof(short_fields); ++i)
            game_memory_write_u32(memory, node + short_fields[i], 0u);
    } else if (object_kind == 5u) {
        game_memory_write_u8(memory, node + 0x0cu, 0u);
        game_memory_write_u16(memory, node + 0x2cu, 0u);
        game_memory_write_u16(memory, node + 0x38u, 1u);
        for (i = 0; i < sizeof(long_fields); ++i)
            game_memory_write_u32(memory, node + long_fields[i], 0u);
    } else if(object_kind>=1u && object_kind<=3u){
        game_memory_write_u32(memory,node+12u,0u);game_memory_write_u8(memory,node+16u,0u);
        if(object_kind==2u){game_memory_write_u32(memory,node+24u,0u);game_memory_write_u32(memory,node+28u,0u);}
        if(object_kind==3u){game_memory_write_u8(memory,node+24u,0u);for(i=0u;i<4u;i++)game_memory_write_u32(memory,node+26u+i*4u,0u);}
    } else return 0;
    return !memory->fault;
}

int game_effect_resource_before_delete(GameMemory *memory,
        GameAddress node, uint16_t object_kind) {
    if (!memory || (object_kind > 3u && object_kind != 5u)) return 0;
    game_memory_write_u32(memory, node, 0x00572db0u);
    return !memory->fault;
}
