#ifndef UPRISING_NATIVE_DERIVED_STATE_COPY_H
#define UPRISING_NATIVE_DERIVED_STATE_COPY_H

#include "game_memory.h"

int game_derived_state_copy_sparse(GameMemory *memory,
    GameAddress destination, GameAddress source);

#endif
