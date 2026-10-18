#ifndef UPRISING_PLAYER_TERRAIN_H
#define UPRISING_PLAYER_TERRAIN_H

#include "player_state.h"

typedef enum GamePlayerTerrainResult {
    GAME_PLAYER_TERRAIN_CONTINUE = 0,
    GAME_PLAYER_TERRAIN_HEALTH_ZERO = 1,
    GAME_PLAYER_TERRAIN_PLAYER_DIED = 2
} GamePlayerTerrainResult;

typedef struct GamePlayerTerrainOps {
    void *context;
    void (*observe_health)(void *context, uint32_t health);
    int (*truncate_f32)(void *context, GameAddress address,
        uint32_t *result);
    int (*terrain_height)(void *context, float x, float z,
        long double *result);
    int (*clearance_below_limit)(void *context, GamePlayerRef player,
        long double terrain_height, int *below);
    int (*apply_damage)(void *context, GamePlayerRef player,
        GamePlayerTerrainResult *result);
    int (*emit_effect)(void *context, GamePlayerRef player,
        GameAddress scratch_point);
} GamePlayerTerrainOps;

int game_player_update_terrain_hazard(GamePlayerRef player,
    GameAddress scratch_point, const GamePlayerTerrainOps *ops,
    GamePlayerTerrainResult *result);

#endif
