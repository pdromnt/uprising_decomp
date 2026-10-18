#ifndef UPRISING_LEVEL_EFFECTS_H
#define UPRISING_LEVEL_EFFECTS_H

#include <stdint.h>
#include "effect_factory.h"

/* A pooled scene effect owns its motion and lifetime between insertion and
 * reset. The guest object is a projection for the remaining scene services. */
typedef struct LevelEffect {
    uint32_t object, resource, birth_tick, lifetime, flags;
    float position[3], velocity[3];
} LevelEffect;

typedef enum LevelEffectStep {
    LEVEL_EFFECT_MISSING = 0,
    LEVEL_EFFECT_ALIVE,
    LEVEL_EFFECT_EXPIRED
} LevelEffectStep;

typedef struct LevelEffectsStats {
    uint32_t created, updates, expired, retired, discarded, live;
} LevelEffectsStats;

int level_effects_begin(const LevelEffect *effect);
int level_effects_get(uint32_t object, LevelEffect *effect);
LevelEffectStep level_effects_step(uint32_t object, uint32_t tick,
    float damping, float gravity, LevelEffect *effect);
void level_effects_retire(uint32_t object);
void level_effects_clear(void);
void level_effects_stats(LevelEffectsStats *stats);

typedef struct LevelEffectRequest {
    uint32_t identity;
    float position[3],velocity[3],scale;
    GameAddress recipe;
    uint16_t angles[3];
} LevelEffectRequest;
typedef struct LevelEffectServices {
    EffectFactoryServices factory;
    int (*place)(void *,GameAddress,const LevelEffectRequest *);
    int (*reset)(void *,GameAddress);
    int (*sound)(void *,int32_t,const float position[3]);
} LevelEffectServices;
int level_effect_initialize(GameMemory *,GameAddress,const LevelEffectRequest *,const LevelEffectServices *);
/* -1 failure, 0 rejected/missing, 1 emitted/updated. */
int level_effect_emit(GameMemory *,GameAddress owner,const LevelEffectRequest *,const LevelEffectServices *);
int level_effect_update(GameMemory *,GameAddress,const LevelEffectServices *);
int level_effect_fragment_event(GameMemory *,GameAddress,uint8_t,const LevelEffectServices *);

#endif
