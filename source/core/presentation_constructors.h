#ifndef UPRISING_NATIVE_PRESENTATION_CONSTRUCTORS_H
#define UPRISING_NATIVE_PRESENTATION_CONSTRUCTORS_H

#include "game_memory.h"

int game_presentation_layout_initialize(GameMemory *memory,
    GameAddress owner);
int game_presentation_layout_bind(GameMemory *memory,
    GameAddress owner, uint32_t resource);
int game_presentation_node_initialize(GameMemory *memory,
    GameAddress frame, GameAddress owner);
int game_presentation_node_clear_tail(GameMemory *memory,
    GameAddress owner);
int game_named_surface_initialize(GameMemory *memory,
    GameAddress owner);
int game_named_surface_configure(GameMemory *memory,
    GameAddress owner, uint32_t width, uint32_t height);

#endif
