#ifndef UPRISING_NATIVE_CRT_HANDLES_H
#define UPRISING_NATIVE_CRT_HANDLES_H

#include "game_memory.h"

/* Eight-byte CRT descriptor: invalid OS handle, no flags, text mode. */
int game_crt_handle_initialize(GameMemory *memory, GameAddress record);

#endif
