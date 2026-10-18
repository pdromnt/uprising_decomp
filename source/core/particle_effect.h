#ifndef UPRISING_PARTICLE_EFFECT_H
#define UPRISING_PARTICLE_EFFECT_H
#include "effect_initialization.h"

typedef struct ParticleEffect {
    uint32_t birth_tick;
    float initial_position[3];
    int16_t lifetime;
    uint16_t restarts;
    GameAddress recipe;
} ParticleEffect;
typedef struct ParticleEffectServices {
    void *context;
    int (*place)(void *,uint32_t identity,const float position[3]);
    int (*inside)(void *,int *);
    int (*reinsert)(void *);
    int (*deactivate)(void *);
} ParticleEffectServices;
int particle_effect_emission_allowed(GameMemory *,float radius,const float position[3]);
int particle_effect_surface_allowed(GameMemory *,float radius,const float position[3],int32_t chance);
int particle_effect_import(GameMemory *,GameAddress,ParticleEffect *);
int particle_effect_projection_read(ParticleEffect *,uint32_t,void *,size_t);
int particle_effect_projection_write(ParticleEffect *,uint32_t,const void *,size_t);
int particle_effect_initialize(GameMemory *,ParticleEffect *,PresentationPose *,EffectModel *,
    uint32_t identity,const float position[3],GameAddress recipe,const ParticleEffectServices *);
int particle_effect_update(GameMemory *,ParticleEffect *,PresentationPose *,EffectModel *,uint32_t tick,
    const ParticleEffectServices *);
#endif
