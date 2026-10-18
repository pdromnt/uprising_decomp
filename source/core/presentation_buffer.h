#ifndef UPRISING_NATIVE_PRESENTATION_BUFFER_H
#define UPRISING_NATIVE_PRESENTATION_BUFFER_H

#include "game_memory.h"

void presentation_buffer_flip_and_compact(GameMemory *memory,
    GameAddress descriptor, GameAddress scratch);

#endif
