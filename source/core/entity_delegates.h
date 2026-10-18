#ifndef UPRISING_NATIVE_ENTITY_DELEGATES_H
#define UPRISING_NATIVE_ENTITY_DELEGATES_H

#include "game_memory.h"

#include <stdint.h>

GameAddress entity_delegate(GameMemory *memory, GameAddress owner);
GameAddress entity_active_delegate(GameMemory *memory, GameAddress owner);
uint32_t entity_delegate_property(GameMemory *memory, GameAddress owner);
GameAddress entity_delegate_virtual_target(GameMemory *memory,
    GameAddress owner, uint32_t slot);
GameAddress entity_delegate_for_generation(GameMemory *memory,
    GameAddress owner, uint32_t requested);
uint32_t entity_add_delegate_property(GameMemory *memory,
    GameAddress owner, uint32_t amount);
void entity_reset_binding(GameMemory *memory, GameAddress owner);
uint32_t entity_query_metric_state(GameMemory *memory, GameAddress owner,
    GameAddress *delegate);
void entity_publish_display_value(GameMemory *memory, GameAddress owner);
GameAddress entity_delegate_static_table(void);
uint8_t entity_state_byte(GameMemory *memory, GameAddress owner,
    uint32_t offset);

#endif
