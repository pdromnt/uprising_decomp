#ifndef UPRISING_NATIVE_FRAME_PRESENT_H
#define UPRISING_NATIVE_FRAME_PRESENT_H

#include "game_memory.h"

typedef struct GameFramePresentOps {
    void *context;
    int (*present)(void *context, uint32_t interval);
    int (*call)(void *context, GameAddress target,
        const uint32_t *arguments, unsigned count, uint32_t *result);
} GameFramePresentOps;

typedef struct GameFramePresentResult {
    uint32_t now;
    uint32_t start;
} GameFramePresentResult;

int game_frame_present(GameMemory *memory, const GameFramePresentOps *ops,
    GameFramePresentResult *result);

#endif
