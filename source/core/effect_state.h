#ifndef UPRISING_NATIVE_EFFECT_STATE_H
#define UPRISING_NATIVE_EFFECT_STATE_H

#include "game_memory.h"

int game_random_effect_begin(GameMemory *memory, GameAddress object);
int game_random_effect_clear_vectors(GameMemory *memory,
    GameAddress object);
int game_effect_pool_node_finish(GameMemory *memory, GameAddress node);
int game_effect_resource_initialize(GameMemory *memory, GameAddress node,
    uint16_t object_kind);
int game_effect_resource_reset(GameMemory *memory, GameAddress node,
    uint16_t object_kind);
int game_effect_resource_before_delete(GameMemory *memory,
    GameAddress node, uint16_t object_kind);

#endif
