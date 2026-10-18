#ifndef UPRISING_SPRITE_FRAGMENT_H
#define UPRISING_SPRITE_FRAGMENT_H
#include "presentation_instance.h"
typedef struct FragmentRequest {
    uint32_t identity;float position[3];uint16_t angle;
    GameAddress profile;float scale,velocity[3];
    int32_t lifetime;uint32_t source_identity;uint8_t flags;
} FragmentRequest;
typedef struct FragmentServices {
    void *context;
    int (*height)(void *,float,float,long double *);
    int (*place)(void *,GameAddress,const FragmentRequest *);
    int (*deactivate)(void *,GameAddress);
    int (*terrain_event)(void *,GameAddress,uint8_t);
    int (*sound)(void *,GameAddress,int16_t,const float[3]);
} FragmentServices;
int fragment_begin(GameMemory *,GameAddress,const FragmentRequest *,const FragmentServices *);
int fragment_prepare(GameMemory *,GameAddress,const FragmentServices *);
int fragment_build_world(GameMemory *,GameAddress);
int fragment_build_model(GameMemory *,GameAddress);
int fragment_move(GameMemory *,GameAddress);
int fragment_terrain_response(GameMemory *,GameAddress,const FragmentServices *);
int fragment_expire(GameMemory *,GameAddress,const FragmentServices *);
int fragment_update(GameMemory *,GameAddress,const FragmentServices *);
int fragment_motion(GameMemory *,GameAddress,const FragmentServices *);
int fragment_terrain_contact(GameMemory *,const PresentationPose *,int *);
GameAddress fragment_profile(GameMemory *,int16_t selector);
typedef struct FragmentSpawnServices {
    FragmentServices fragment;
    int (*visible)(void *,GameAddress,int *);
    GameAddress (*acquire)(void *);
    int (*sequence)(void *,GameAddress,uint32_t *);
    int (*publish)(void *,GameAddress,GameAddress);
    int (*destroy)(void *,GameAddress);
} FragmentSpawnServices;
int fragment_spawn(GameMemory *,GameAddress,int32_t,const float[3],const FragmentSpawnServices *);
int fragment_mesh_spawn(GameMemory *,GameAddress,GameAddress,const float[12],
    const float[3],int32_t,int16_t,const FragmentSpawnServices *);
#endif
