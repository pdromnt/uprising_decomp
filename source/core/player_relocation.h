#ifndef UPRISING_PLAYER_RELOCATION_H
#define UPRISING_PLAYER_RELOCATION_H
#include "player_rig.h"

typedef struct PlayerRelocationServices {
    void *context;
    int (*height)(void *,float x,float z,long double *height);
    int (*destination)(void *,float destination[2]);
    int (*clear)(void *,const float position[3],int *clear);
    int (*angle)(void *,int32_t *angle);
    float (*sine)(void *,unsigned angle);
} PlayerRelocationServices;

/* The caller emits departure before choosing the destination. A rejected
 * destination falls back to the current pose; obstructed valid destinations
 * expand the random search in groups of twenty attempts. */
int player_relocation_choose(PresentationPose *,float radius_step,
    const PlayerRelocationServices *,float position[3]);
int player_relocation_around(const float destination[2],float radius,
    int32_t angle,const PlayerRelocationServices *,float position[2]);
int player_relocation_position_clear(GameMemory *,GameAddress manager,
    float radius,const float limits[3],const float position[3],int *clear);
typedef struct PlayerSpawnServices {
    void *context;
    int (*height)(void *,float x,float z,float *detail,long double *terrain);
    int (*clear)(void *,const float position[3],int *clear);
    float (*sine)(void *,unsigned phase);
} PlayerSpawnServices;
int player_spawn_choose(const float requested[3],float clearance,float ring_scale,
    const PlayerSpawnServices *,float position[3]);

typedef struct EntityArrivalAction {
    GameAddress target;
    uint32_t identity,mode;
} EntityArrivalAction;
typedef struct EntityArrivalServices {
    void *context;
    int (*identity)(void *,GameAddress,uint32_t *identity);
    int (*clear_flag)(void *,GameAddress,uint32_t flag);
    int (*remove)(void *,GameAddress);
    int (*mark_removed)(void *,uint32_t identity);
    int (*health)(void *,GameAddress,int32_t *health);
    int (*move)(void *,GameAddress,const float position[3]);
} EntityArrivalServices;
int entity_arrival_finish(EntityArrivalAction *,const float position[3],
    const EntityArrivalServices *);
#endif
