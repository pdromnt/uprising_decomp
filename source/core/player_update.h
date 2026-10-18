#ifndef UPRISING_PLAYER_UPDATE_H
#define UPRISING_PLAYER_UPDATE_H

#include "player_death.h"
#include "player_frame.h"
#include "player_terrain.h"
#include "player_weapons.h"

typedef struct GamePlayerUpdateOps {
    void *context;
    void (*before_frame)(void *context, GameAddress player);
    const GamePlayerFrameCallbacks *frame;
    const GamePlayerWeaponCallbacks *weapons;
    const GamePlayerDeathOps *death;
    const GamePlayerTerrainOps *terrain;
    GameMatrixCompose matrix_compose;
    void *matrix_context;
} GamePlayerUpdateOps;

int game_player_update_phases(GamePlayerRef player, uint32_t initial_health,
    GameAddress low_frame, const GamePlayerUpdateOps *ops);

#endif
