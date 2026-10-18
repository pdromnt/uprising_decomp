#ifndef RECOMP_GAME_STRING_SEARCH_H
#define RECOMP_GAME_STRING_SEARCH_H

#include "game_memory.h"

/* Guest-address strchr; finding a NUL returns its address, as in C. */
GameAddress game_string_find_byte(GameMemory *memory, GameAddress string,
    uint8_t needle);

#endif
