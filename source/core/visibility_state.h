#ifndef UPRISING_NATIVE_VISIBILITY_STATE_H
#define UPRISING_NATIVE_VISIBILITY_STATE_H

#include "game_memory.h"

int game_visibility_state_initialize_begin(GameMemory *memory,
    GameAddress object);
int game_visibility_state_initialize_limits(GameMemory *memory,
    GameAddress object);

#endif
