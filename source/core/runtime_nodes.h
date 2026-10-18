#ifndef UPRISING_NATIVE_RUNTIME_NODES_H
#define UPRISING_NATIVE_RUNTIME_NODES_H

#include "game_memory.h"

int game_runtime_node_reset_links(GameMemory *memory, GameAddress node);
int game_runtime_node_initialize_fields(GameMemory *memory,
    GameAddress node);
int game_runtime_node_initialize(GameMemory *memory, GameAddress node);
int game_runtime_pool_clear(GameMemory *memory, GameAddress owner,
    int reverse);

#endif
