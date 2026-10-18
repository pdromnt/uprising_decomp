#ifndef UPRISING_EFFECT_INITIALIZATION_H
#define UPRISING_EFFECT_INITIALIZATION_H
#include "effect_animation.h"
#include "effect_model.h"

typedef struct EffectInitialization {
    uint32_t identity;
    uint16_t angle;
    GameAddress attachment,profile;
    uint8_t subtype,placement_flags;
    float scale[2],radius,position[3];
    uint32_t contact_armed,loop_control;
    GameAddress emitter;
    int16_t mark_delay,mark_kind;
    uint32_t mark_radius;
} EffectInitialization;

typedef struct EffectInitializationServices {
    void *context;
    int (*terrain_height)(void *,float x,float z,long double *);
    int (*place)(void *,uint32_t identity,const float position[3],uint16_t angle);
} EffectInitializationServices;
uint32_t effect_initialization_random(GameMemory *,float scale);
GameAddress effect_initialization_profile(GameMemory *,unsigned selector);
int effect_initialize(GameMemory *,EffectAnimation *,PresentationPose *,EffectModel *,
    EffectInitialization *,const EffectInitializationServices *);
typedef struct EffectBurstServices {
    void *context;
    GameAddress (*acquire)(void *);
    int (*emit)(void *,GameAddress,GameAddress,EffectInitialization *);
    int (*sound)(void *,GameAddress,int16_t);
    int (*discard)(void *,GameAddress);
} EffectBurstServices;
int effect_burst(GameMemory *,GameAddress,uint16_t,int32_t,int16_t,const EffectBurstServices *);
/* Transition actions retain a target identity until completion. Foreign
 * catalogs, target constructors and delegate creation are explicit ports. */
typedef struct EffectTransitionServices {
    EffectInitializationServices initialization;
    int (*lookup)(void *,GameAddress world,uint32_t identity,GameAddress *);
    int (*sequence)(void *,GameAddress world,const float position[3],uint16_t angle,uint32_t *);
    int (*bind)(void *,GameAddress target,uint32_t identity);
    int (*register_target)(void *,GameAddress catalog,GameAddress target);
    int (*publish)(void *,GameAddress world,GameAddress target);
    int (*delegate)(void *,GameAddress world,GameAddress target);
} EffectTransitionServices;
int effect_initialize_transition(GameMemory *,EffectAnimation *,PresentationPose *,EffectModel *,
    uint32_t sequence,const float position[3],uint16_t status,GameAddress target,
    uint32_t mode,uint32_t request,const EffectTransitionServices *);
#endif
