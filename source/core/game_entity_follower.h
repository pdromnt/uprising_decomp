#ifndef RECOMP_GAME_ENTITY_FOLLOWER_H
#define RECOMP_GAME_ENTITY_FOLLOWER_H

#include "game_memory.h"

typedef int (*GameFollowerCall)(void *context, GameAddress target,
    GameAddress object, const uint32_t *arguments, unsigned count,
    uint32_t *result, int use_scratch_stack);

/* Returns zero when a callback or memory access aborts the guest update. */
int game_entity_follower_update(GameMemory *memory, GameAddress object,
    GameAddress scratch, GameFollowerCall call, void *context);

#endif
