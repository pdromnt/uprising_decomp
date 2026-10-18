#ifndef UPRISING_EXPANSION_EFFECT_H
#define UPRISING_EXPANSION_EFFECT_H
#include "effect_model.h"
#include "presentation_pose.h"

typedef struct ExpansionEffect {
    uint32_t phase,step;
    float width,height,top;
    int32_t duration;
} ExpansionEffect;
typedef struct ExpansionEffectServices {
    void *context;
    int (*place)(void *,uint32_t identity,const float position[3]);
    int (*detach)(void *);
    int (*insert)(void *);
    int (*deactivate)(void *);
} ExpansionEffectServices;
int expansion_effect_import(GameMemory *,GameAddress,ExpansionEffect *);
int expansion_effect_projection_read(ExpansionEffect *,uint32_t,void *,size_t);
int expansion_effect_projection_write(ExpansionEffect *,uint32_t,const void *,size_t);
int expansion_effect_initialize(GameMemory *,ExpansionEffect *,PresentationPose *,EffectModel *,
    uint32_t identity,const float position[3],float width,float height,int32_t duration,const ExpansionEffectServices *);
int expansion_effect_update(GameMemory *,ExpansionEffect *,PresentationPose *,EffectModel *,const ExpansionEffectServices *);
void expansion_effect_shrink(ExpansionEffect *);
void expansion_effect_remove(ExpansionEffect *);
#endif
