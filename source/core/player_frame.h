#ifndef UPRISING_PLAYER_FRAME_H
#define UPRISING_PLAYER_FRAME_H

#include "event_queue.h"
#include "player_state.h"

typedef struct GamePlayerFrameCallbacks {
    void *context;
    int (*health)(void *context, GameMemory *memory, GameAddress player,
        uint32_t *result);
    int (*spatial_unlink)(void *context, GameAddress player);
    int (*sample_animation)(void *context, GameAddress player);
    int (*process_collisions)(void *context, GameAddress player,
        uint32_t *result);
    int (*spatial_insert)(void *context, GameAddress player);
    GameEventQueueCallbacks events;
} GamePlayerFrameCallbacks;

typedef enum GamePlayerFrameGate {
    GAME_PLAYER_FRAME_FAULT = -1,
    GAME_PLAYER_FRAME_STOP = 0,
    GAME_PLAYER_FRAME_CONTINUE = 1
} GamePlayerFrameGate;

GamePlayerFrameGate game_player_prepare_frame(GamePlayerRef player,
    const GamePlayerFrameCallbacks *callbacks);

#endif
