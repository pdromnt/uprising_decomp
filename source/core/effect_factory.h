#ifndef UPRISING_EFFECT_FACTORY_H
#define UPRISING_EFFECT_FACTORY_H
#include "presentation_instance.h"
typedef struct EffectFamily {
    uint8_t kind;
    GameAddress publication,dimensions,methods,load_assets;
    uint32_t flags,size;
    unsigned model_mode;
} EffectFamily;
typedef struct EffectFactoryServices {
    void *context;
    int (*load_assets)(void *,GameAddress);
    GameAddress (*allocate)(void *,uint32_t);
    int (*destroy)(void *,GameAddress);
} EffectFactoryServices;
const EffectFamily *effect_factory_family(unsigned kind);
const EffectFamily *effect_factory_family_slot(unsigned kind,uint16_t slot);
GameAddress effect_factory_template(GameMemory *,const EffectFamily *,const EffectFactoryServices *);
int effect_factory_fill(GameMemory *,const EffectFamily *,int32_t,const EffectFactoryServices *);
GameAddress effect_factory_take(GameMemory *,const EffectFamily *,const EffectFactoryServices *);
int effect_factory_shutdown(GameMemory *,const EffectFamily *,const EffectFactoryServices *);
#endif
