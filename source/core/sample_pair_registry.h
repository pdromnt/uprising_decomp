#ifndef UPRISING_NATIVE_SAMPLE_PAIR_REGISTRY_H
#define UPRISING_NATIVE_SAMPLE_PAIR_REGISTRY_H

#include "game_memory.h"

typedef struct GameSamplePairSelection {
    int32_t index;
    int used_search;
} GameSamplePairSelection;

GameSamplePairSelection game_sample_pair_register(GameMemory *memory,
    int32_t requested, uint32_t first, uint32_t second);

#endif
