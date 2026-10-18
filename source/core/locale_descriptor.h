#ifndef UPRISING_NATIVE_LOCALE_DESCRIPTOR_H
#define UPRISING_NATIVE_LOCALE_DESCRIPTOR_H

#include "game_memory.h"

/* Retail's 61 dwords plus a word and byte of descriptor workspace. */
int game_locale_descriptor_clear(GameMemory *memory,
    GameAddress workspace, int reverse);

#endif
