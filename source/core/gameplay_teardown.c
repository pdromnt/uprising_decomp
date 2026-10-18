#include "gameplay_teardown.h"
#include "game_intrusive_list.h"

static const GameplayTeardownSlotSpec resource_owner_slots[] = {
    {0x58u, 0x0051d41fu},
    {0x5cu, 0x0051d436u},
    {0x60u, 0x0051d44du},
    {0x8cu, 0x0051d467u}
};

static const GameplayTeardownGlobalSpec service_globals[] = {
    {0x00597f64u, 0x00517841u},
    {0x00626e78u, 0x0051785cu},
    {0x006278f8u, 0x0051787fu},
    {0x006278fcu, 0x0051787fu},
    {0x0062694cu, 0x0051789fu},
    {0x00626e58u, 0x005178b8u}
};

static const GameplayTeardownListSpec owner_lists[] = {
    {0x1cu, 0x00478a2eu, 0x00478a3bu, 0x00478a40u},
    {0x80u, 0x00478a55u, 0x00478a62u, 0x00478a67u},
    {0xe4u, 0x00478a7cu, 0x00478a89u, 0x00478a8eu}
};


GameplayTeardownSlotSpec gameplay_teardown_resource_owner_slot(
    unsigned index) {
    GameplayTeardownSlotSpec empty = {0u, 0u};
    if (index >= sizeof(resource_owner_slots) / sizeof(resource_owner_slots[0]))
        return empty;
    return resource_owner_slots[index];
}

GameplayTeardownGlobalSpec gameplay_teardown_service_global(unsigned index) {
    GameplayTeardownGlobalSpec empty = {0u, 0u};
    if (index >= sizeof(service_globals) / sizeof(service_globals[0]))
        return empty;
    return service_globals[index];
}

GameplayTeardownListSpec gameplay_teardown_owner_list(unsigned index) {
    GameplayTeardownListSpec empty = {0u, 0u, 0u, 0u};
    if (index >= sizeof(owner_lists) / sizeof(owner_lists[0])) return empty;
    return owner_lists[index];
}


GameplayRuntimeTeardownSpec gameplay_teardown_runtime_service_spec(void) {
    GameplayRuntimeTeardownSpec spec = {
        100u, 0x01000400u, 0x0062707cu, 0x00626e78u, 0x00626d74u
    };
    return spec;
}

uint32_t gameplay_teardown_component_slot_offset(unsigned index) {
    return index < 15u ? 8u + index * 4u : 0u;
}

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

void gameplay_teardown_clear_entity_binding(GameMemory *memory,
    GameAddress owner) {
    game_memory_write_u32(memory, owner + 0xf4u, 0u);
    game_memory_write_u32(memory, owner + 0xf8u, 0u);
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

void gameplay_teardown_mark_entity_auxiliary_empty(GameMemory *memory,
    GameAddress owner) {
    game_memory_write_u32(memory, owner + 0x4cu, 3u);
}

void gameplay_teardown_detach_entity_auxiliary(GameMemory *memory,
    GameAddress owner) {
    GameAddress entity = game_memory_read_u32(memory, owner + 0x3cu);
    if (memory->fault || entity == 0u) return;
    game_memory_write_u32(memory, entity + 0xb4u, 0u);
    game_memory_write_u32(memory, entity + 0xd4u, 0u);
}

void gameplay_teardown_clear_presentation_surface(GameMemory *memory,
    GameAddress owner, unsigned slot) {
    static const uint32_t offsets[] = {0x44u, 0xdcu};
    if (slot < sizeof(offsets) / sizeof(offsets[0]))
        game_memory_write_u32(memory, owner + offsets[slot], 0u);
}

void gameplay_teardown_reset_presentation_owner(GameMemory *memory,
    GameAddress owner) {
    static const uint32_t offsets[] = {0x5cu, 0x98u, 0x9cu, 0x58u, 0x60u};
    unsigned index;
    for (index = 0u; index < sizeof(offsets) / sizeof(offsets[0]); ++index)
        game_memory_write_u32(memory, owner + offsets[index], 0u);
}

void gameplay_teardown_mark_presentation_owner_inactive(GameMemory *memory,
    GameAddress owner) {
    game_memory_write_u32(memory, owner + 0x18u, 1u);
}

void gameplay_teardown_clear_presentation_tail(GameMemory *memory,
    GameAddress owner, unsigned slot) {
    static const uint32_t offsets[] = {0xa8u, 0xc8u};
    if (slot < sizeof(offsets) / sizeof(offsets[0]))
        game_memory_write_u32(memory, owner + offsets[slot], 0u);
}

void gameplay_teardown_clear_service_global(GameMemory *memory,
    unsigned index) {
    GameplayTeardownGlobalSpec spec = gameplay_teardown_service_global(index);
    if (spec.slot != 0u) game_memory_write_u32(memory, spec.slot, 0u);
    if (index == 1u) {
        game_memory_write_u32(memory, 0x0062707cu, 0u);
        game_memory_write_u32(memory, 0x00626d58u, 0u);
    }
}
