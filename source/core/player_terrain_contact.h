#ifndef UPRISING_NATIVE_PLAYER_TERRAIN_CONTACT_H
#define UPRISING_NATIVE_PLAYER_TERRAIN_CONTACT_H

#include "player_rig.h"

#include <stdint.h>

typedef struct GamePlayerTerrainContactOps {
    void *context;
    int (*landing_sound)(void *context, GameAddress sound_manager,
        const uint32_t arguments[6]);
} GamePlayerTerrainContactOps;

int player_update_terrain_contact(GameMemory *memory,
    PresentationPose *pose, PlayerControls *controls, PlayerRig *rig,
    float clearance, uint32_t initial_result,
    const GamePlayerTerrainContactOps *ops, uint32_t *final_result);

#endif
