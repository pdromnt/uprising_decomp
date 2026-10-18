#ifndef UPRISING_NATIVE_RESOURCE_REFRESH_H
#define UPRISING_NATIVE_RESOURCE_REFRESH_H

#include "game_memory.h"

typedef struct GameResourceRefreshOps {
    void *context;
    int (*prepare)(void *context, uint32_t manager, uint32_t current);
    int (*release)(void *context);
    int (*load)(void *context, uint32_t *resource);
} GameResourceRefreshOps;

int game_resource_refresh(GameMemory *memory,
    const GameResourceRefreshOps *ops);

#endif
