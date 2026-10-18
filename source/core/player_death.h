#ifndef UPRISING_PLAYER_DEATH_H
#define UPRISING_PLAYER_DEATH_H

#include "player_state.h"

typedef enum GamePlayerDeathResult {
    GAME_PLAYER_DEATH_CONTINUE = 0,
    GAME_PLAYER_DEATH_FINALIZED = 1
} GamePlayerDeathResult;

typedef int (*GamePlayerDeathAction)(void *context, GamePlayerRef player,
    GameAddress guest_stack_top);
typedef int (*GamePlayerFinalizeAction)(void *context, GameAddress player);

typedef struct GamePlayerDeathOps {
    void *context;
    GamePlayerDeathAction make_effect;
    GamePlayerDeathAction play_sound;
    GamePlayerFinalizeAction finalize;
} GamePlayerDeathOps;

int game_player_update_delayed_death(GamePlayerRef player,
    GameAddress guest_stack_top, const GamePlayerDeathOps *ops,
    GamePlayerDeathResult *result);

#endif
