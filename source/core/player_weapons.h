#ifndef UPRISING_PLAYER_WEAPONS_H
#define UPRISING_PLAYER_WEAPONS_H

#include "player_state.h"

typedef struct GamePlayerWeaponCallbacks {
    void *context;
    int (*fire)(void *context, GameMemory *memory, GameAddress weapon,
        GameAddress firing_matrix, uint32_t *result);
} GamePlayerWeaponCallbacks;

typedef int (*GameMatrixCompose)(void *context, GameAddress output,
    GameAddress left, GameAddress right);

int game_player_build_firing_transform(GamePlayerRef player,
    GameMatrixCompose compose, void *compose_context,
    GameAddress intermediate_matrix, GameAddress firing_matrix);
int game_player_update_weapons(GamePlayerRef player,
    const GamePlayerWeaponCallbacks *callbacks,
    GameMatrixCompose compose, void *compose_context,
    GameAddress intermediate_matrix, GameAddress firing_matrix);

#endif
