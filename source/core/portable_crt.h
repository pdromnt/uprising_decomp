#ifndef UPRISING_NATIVE_PORTABLE_CRT_H
#define UPRISING_NATIVE_PORTABLE_CRT_H

#include "game_memory.h"

#include <stdint.h>

GameAddress portable_crt_strrchr(GameMemory *memory, GameAddress string,
    uint8_t target);
int32_t portable_crt_memcmp(GameMemory *memory, GameAddress left,
    GameAddress right, uint32_t count);
uint32_t portable_crt_rand_step(GameMemory *memory);
void portable_crt_rand_seed(GameMemory *memory,uint32_t seed);
void portable_crt_strncpy(GameMemory *memory, GameAddress destination,
    GameAddress source, uint32_t count);
uint32_t portable_crt_strnlen(GameMemory *memory, GameAddress string,
    uint32_t limit);
GameAddress portable_crt_strpbrk(GameMemory *memory, GameAddress string,
    GameAddress accept);
GameAddress portable_crt_mbsrchr(GameMemory *memory, GameAddress string,
    uint32_t target);

#endif
