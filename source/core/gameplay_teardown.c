#include "gameplay_teardown.h"
#include "game_intrusive_list.h"

GameAddress gameplay_teardown_begin_scene_object(GameMemory *memory,
    GameAddress object) {
    GameAddress next, previous;
    game_memory_write_u32(memory, object, 0x005721b8u);
    game_memory_write_u32(memory, object + 0x58u, 0u);
    next = game_memory_read_u32(memory, object + 8u);
    if (!memory->fault && next)
        game_list_detach_node(memory, object, 4u, 8u);
    previous = game_memory_read_u32(memory, object + 0x4cu);
    if (!memory->fault && previous)
        game_list_detach_node(memory, object, 0x4cu, 0x48u);
    if (memory->fault) return 0;
    return game_memory_read_u32(memory, object + 0x54u);
}

GameplaySceneObjectMembers gameplay_teardown_scene_object_members(
    GameMemory *memory, GameAddress object) {
    GameplaySceneObjectMembers result = {0u, 0u};
    GameAddress retained;
    result.buffer = game_memory_read_u32(memory, object + 0x40u);
    retained = game_memory_read_u32(memory, object + 0x50u);
    if (!memory->fault && !retained)
        result.auxiliary = game_memory_read_u32(memory, object + 0x3cu);
    return result;
}

void gameplay_teardown_finish_scene_object(GameMemory *memory,
    GameAddress object) {
    game_memory_write_u32(memory, object, 0x00572230u);
}

void gameplay_teardown_reset_entity_auxiliary(GameMemory *memory,
    GameAddress owner, unsigned stage) {
    static const uint32_t offsets[][2] = {
        {0x500u, 0x504u}, {0x508u, 0x1f8u}
    };
    unsigned index;
    if (stage >= sizeof(offsets) / sizeof(offsets[0])) return;
    for (index = 0u; index < 2u; ++index)
        game_memory_write_u32(memory, owner + offsets[stage][index], 0u);
}
