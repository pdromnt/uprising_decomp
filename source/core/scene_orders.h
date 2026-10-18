#ifndef UPRISING_SCENE_ORDERS_H
#define UPRISING_SCENE_ORDERS_H
#include "game_memory.h"
#include "presentation_commands.h"
typedef struct SceneOrderServices {
    void *context;
    int (*acquire)(void *,GameAddress,GameAddress *);
    int (*issue)(void *,GameAddress,const PresentationCommand *);
    int (*advance)(void *,GameAddress,int cancel_route);
    int (*speed)(void *,GameAddress,long double *);
    int (*move)(void *,GameAddress,float x,float z);
    int (*census)(void *,GameAddress,uint8_t type,int observed);
    int (*other)(void *,GameAddress,GameAddress method);
    int (*line)(void *,const float from[3],const float to[3],int *clear);
    int (*attack)(void *,GameAddress,GameAddress target);
    int (*target_order)(void *,GameAddress,uint32_t priority,GameAddress target);
    int (*identity_order)(void *,GameAddress,uint32_t priority,uint32_t identity);
    int (*target_priority)(void *,GameAddress,GameAddress root,GameAddress target,int32_t *);
    int (*notice)(void *,GameAddress,uint32_t reason);
    int (*capture)(void *,GameAddress group,GameAddress owner,uint16_t *status);
    int (*publish)(void *,GameAddress);
    int (*landing_sound)(void *,GameAddress,int ending);
} SceneOrderServices;
int scene_order_tick(GameMemory *,GameAddress,const SceneOrderServices *);
int scene_order_execute(GameMemory *,GameAddress,GameAddress,const SceneOrderServices *);
int scene_order_segment_clear(GameMemory *,const float from[3],const float to[3]);
int scene_order_position_clear(GameMemory *,GameAddress,float,float,int *clear);
int scene_order_destination(GameMemory *,GameAddress,const float[3],int *result);
int scene_order_steer(GameMemory *,GameAddress,const float point[3],const SceneOrderServices *,int *moved);
int scene_order_move(GameMemory *,GameAddress,GameAddress method,float,float,const SceneOrderServices *);
int scene_order_interaction(GameMemory *,GameAddress,GameAddress method,const SceneOrderServices *,int *result);
typedef struct SceneReactionServices {
    SceneOrderServices orders;
    int (*maximum_health)(void *,GameAddress,uint32_t *);
    int (*issue)(void *,GameAddress,const PresentationCommand *,int target_only);
} SceneReactionServices;
int scene_order_reaction(GameMemory *,GameAddress,GameAddress attacker,int32_t damage,const SceneReactionServices *);
#endif
