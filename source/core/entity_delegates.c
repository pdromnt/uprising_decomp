#include "entity_delegates.h"

GameAddress entity_delegate(GameMemory *memory, GameAddress owner) {
    return game_memory_read_u32(memory, owner + 0x48u);
}

GameAddress entity_active_delegate(GameMemory *memory, GameAddress owner) {
    GameAddress delegate = entity_delegate(memory, owner);
    uint32_t state = game_memory_read_u32(memory, delegate + 0x14u);
    if (memory->fault || state < 1u) return 0u;
    return delegate;
}

uint32_t entity_delegate_property(GameMemory *memory, GameAddress owner) {
    GameAddress delegate = entity_delegate(memory, owner);
    return game_memory_read_u32(memory, delegate + 0x58u);
}

GameAddress entity_delegate_virtual_target(GameMemory *memory,
    GameAddress owner, uint32_t slot) {
    GameAddress delegate = entity_delegate(memory, owner);
    GameAddress vtable = game_memory_read_u32(memory, delegate);
    return game_memory_read_u32(memory, vtable + slot);
}

GameAddress entity_delegate_for_generation(GameMemory *memory,
    GameAddress owner, uint32_t requested) {
    GameAddress delegate = entity_delegate(memory, owner);
    uint32_t state = game_memory_read_u32(memory, delegate + 0x14u);
    if (memory->fault || state - requested >= 1u) return 0u;
    return delegate;
}

uint32_t entity_add_delegate_property(GameMemory *memory,
    GameAddress owner, uint32_t amount) {
    GameAddress delegate = entity_delegate(memory, owner);
    uint32_t value = game_memory_read_u32(memory, delegate + 0x58u) + amount;
    if (!memory->fault) game_memory_write_u32(memory, delegate + 0x58u, value);
    return value;
}

void entity_reset_binding(GameMemory *memory, GameAddress owner) {
    GameAddress entity = game_memory_read_u32(memory, owner + 0x3cu);
    if (memory->fault || entity == 0u) return;
    game_memory_write_u32(memory, entity + 0xb4u, 0u);
    if (!memory->fault) game_memory_write_u32(memory, entity + 0xd4u, 0u);
}

uint32_t entity_query_metric_state(GameMemory *memory, GameAddress owner,
    GameAddress *delegate) {
    uint32_t state = game_memory_read_u32(memory, owner + 0x10u);
    if (memory->fault) return 0u;
    if (state != 1u) return 100u;
    if (delegate) *delegate = entity_delegate(memory, owner);
    return 0u;
}

void entity_publish_display_value(GameMemory *memory, GameAddress owner) {
    uint32_t value = game_memory_read_u32(memory, 0x005ac6acu);
    GameAddress delegate = entity_delegate(memory, owner);
    if (!memory->fault) game_memory_write_u32(memory, delegate + 0xacu, value);
}

GameAddress entity_delegate_static_table(void) {
    return 0x0059d5dcu;
}

uint8_t entity_state_byte(GameMemory *memory, GameAddress owner,
    uint32_t offset) {
    return game_memory_read_u8(memory, owner + offset);
}
