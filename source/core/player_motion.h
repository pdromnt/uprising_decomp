#ifndef UPRISING_NATIVE_PLAYER_MOTION_H
#define UPRISING_NATIVE_PLAYER_MOTION_H

#include "player_rig.h"

#include <stdint.h>

typedef struct GamePlayerMotionOps {
    void *context;
    int (*update_engine_scalar)(void *context, GameAddress sound_manager,
        uint32_t value_bits);
} GamePlayerMotionOps;

int player_integrate_motion(GameMemory *memory, PresentationPose *pose,
    PlayerControls *controls, PlayerRig *rig, float max_speed,
    const GamePlayerMotionOps *ops);

#endif
