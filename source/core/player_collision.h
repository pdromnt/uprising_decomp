#ifndef UPRISING_NATIVE_PLAYER_COLLISION_H
#define UPRISING_NATIVE_PLAYER_COLLISION_H
#include "player_motion.h"
#include "player_terrain_contact.h"
typedef struct PlayerCollisionParameters {
    float radius,clearance,max_speed;
    int32_t death_tick;
} PlayerCollisionParameters;
typedef struct PlayerCollisionServices {
    void *context;
    GamePlayerMotionOps motion;
    GamePlayerTerrainContactOps terrain;
    int (*query)(void *,const float previous[3],float contact[3],GameAddress excluded,GameAddress *collider);
    void (*observe_terrain)(void *,int entering);
} PlayerCollisionServices;
int player_resolve_movement_collision(GameMemory *,GameAddress identity,
    PresentationPose *,PlayerControls *,PlayerRig *,const PlayerCollisionParameters *,
    const PlayerCollisionServices *,uint32_t *result);
#endif
